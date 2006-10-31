using System;
using System.IO;
using System.Diagnostics;
using System.Collections;
using System.Collections.Generic;
using System.Xml;
using System.Text;
using System.Text.RegularExpressions;
using System.Security.Cryptography;
using Mono.Unix;

namespace FileState
{
	public class StateEntry
	{
		public enum Kind {
			Collection,
			Directory,
			File,
			SymbolicLink,
			Special
		}

		public string Name;
		public string Pathname;
		public Kind   EntryKind;
		public bool   DeletePending;

		public StateEntry Parent;
		public List<StateEntry> Children;

		private FileInfo info;
		public FileInfo Info {
			get {
				if (info == null)
					info = new FileInfo(Pathname);
				return info;
			}
			set {
				info = value;
			}
		}

		private long length = -1;
		public long Length {
			get {
				if (length == -1)
					length = Info.Length;
				return length;
			}
		}

		private static DateTime nullDateTime = new DateTime();

		private DateTime lastWriteTime;
		public DateTime LastWriteTime {
			get {
				if (lastWriteTime == nullDateTime)
					lastWriteTime = Info.LastWriteTime;
				return lastWriteTime;
			}
		}

		private DateTime creationTime;
		public DateTime CreationTime {
			get {
				if (creationTime == nullDateTime)
					creationTime = Info.CreationTime;
				return creationTime;
			}
		}

		private bool attributesRead;
		private FileAttributes attributes;
		public FileAttributes Attributes {
			get {
				if (! attributesRead) {
					attributes = Info.Attributes;
					attributesRead = true;
				}
				return attributes;
			}
		}

		private string checksum;
		public string Checksum {
			get {
				if (checksum != null)
					return checksum;

				if (EntryKind == Kind.File && Info.Exists)
					checksum = ReadChecksum(Pathname);

				return checksum;
			}
			set {
				checksum = value;
			}
		}

		public string CurrentChecksum {
			get { return ReadChecksum(Pathname); }
		}

		public void Reset() {
			Info	 = null;
			Checksum = null;
		}

		public FileState FileStateObj;

		public StateEntry(FileState FileStateObj, StateEntry Parent)
		{
			this.FileStateObj = FileStateObj;
			this.Parent		  = Parent;

			if (Parent != null)
				Parent.AppendChild(this);
		}

		public StateEntry(FileState FileStateObj, StateEntry Parent,
						  string Pathname, string Name, Kind EntryKind)
		: this(FileStateObj, Parent)
		{
			this.Pathname  = Pathname;
			this.Name	   = Name;
			this.EntryKind = EntryKind;
		}

		private void AppendChild(StateEntry entry) {
			if (Children == null)
				Children = new List<StateEntry>();
			Children.Add(entry);
		}

		private static MD5 md5Service = new MD5CryptoServiceProvider();

		static public string ReadChecksum(string path)
		{
			byte[] data;
			try {
				using (FileStream fs = new FileStream(path, FileMode.Open))
					data = md5Service.ComputeHash(fs);

				return BitConverter.ToString(data);
			}
			catch (Exception) {
				return null;
			}
		}

		public void WriteTo(XmlTextWriter w, bool top)
		{
			w.WriteStartElement("entry");

			if (EntryKind != Kind.Collection) {
				w.WriteStartElement("name");
				w.WriteString(top ? Info.FullName : Info.Name);
				w.WriteEndElement();
				top = false;
			}

			w.WriteStartElement("kind");
			w.WriteString(EntryKind.ToString());
			w.WriteEndElement();

			if (EntryKind != Kind.Collection) {
				if (EntryKind != Kind.SymbolicLink && EntryKind != Kind.Special) {
					w.WriteStartElement("creation");
					w.WriteString(CreationTime.ToString());
					w.WriteEndElement();
					w.WriteStartElement("lastwrite");
					w.WriteString(LastWriteTime.ToString());
					w.WriteEndElement();
				}
				w.WriteStartElement("attr");
				w.WriteString(((int)Attributes).ToString());
				w.WriteEndElement();
			}

			if (EntryKind == Kind.File) {
				w.WriteStartElement("length");
				w.WriteString(Length.ToString());
				w.WriteEndElement();

				if (Checksum != null) {
					w.WriteStartElement("md5");
					w.WriteString(Checksum);
					w.WriteEndElement();
				}
			}

			if (Children != null && Children.Count > 0) {
				w.WriteStartElement("children");
				foreach (StateEntry entry in Children)
					entry.WriteTo(w, top);
				w.WriteEndElement();
			}

			w.WriteEndElement();
		}

		public static StateEntry LoadElement(FileState FileStateObj, StateEntry Parent,
											 string parentPath, string relativePath,
											 XmlNode Element)
		{
			StateEntry entry = new StateEntry(FileStateObj, Parent);

			XmlText text = Element.SelectSingleNode("./name/text()") as XmlText;
			if (text != null) {
				if (! Path.IsPathRooted(text.Value))
					entry.Name = (relativePath != null ?
								  Path.Combine(relativePath, text.Value) : text.Value);
				else
					entry.Name = Path.GetFileName(text.Value);

				entry.Pathname = (parentPath != null ?
								  Path.Combine(parentPath, text.Value) : text.Value);
			}

			text = Element.SelectSingleNode("./kind/text()") as XmlText;
			if (text != null)
				entry.EntryKind =
					(StateEntry.Kind)Enum.Parse(typeof(StateEntry.Kind),
												text.Value);

			text = Element.SelectSingleNode("./length/text()") as XmlText;
			if (text != null)
				entry.length = (long)Convert.ToInt32(text.Value);

			text = Element.SelectSingleNode("./lastwrite/text()") as XmlText;
			if (text != null)
				entry.lastWriteTime = Convert.ToDateTime(text.Value);

			text = Element.SelectSingleNode("./creation/text()") as XmlText;
			if (text != null)
				entry.creationTime = Convert.ToDateTime(text.Value);

			text = Element.SelectSingleNode("./attr/text()") as XmlText;
			if (text != null)
				entry.attributes = (FileAttributes)Convert.ToInt32(text.Value);

			text = Element.SelectSingleNode("./md5/text()") as XmlText;
			if (text != null) {
				entry.checksum = text.Value;
				FileStateObj.ChecksumDict[entry.Checksum] = entry;
			}

			XmlNode children = Element.SelectSingleNode("./children");
			if (children != null)
				foreach (XmlNode child in children.ChildNodes)
					LoadElement(FileStateObj, entry, entry.Pathname, entry.Name, child);

			return entry;
		}

		public StateEntry FindChild(string name)
		{
			if (Children != null)
				foreach (StateEntry child in Children)
					if (child.Info.Name == name)
						return child;
			return null;
		}

		public StateEntry FindOrCreateChild(string name)
		{
			StateEntry child = FindChild(name);
			if (child == null) {
				string newPath	  = Path.Combine(Pathname, name);
				string newRelPath = Path.Combine(Name, name);
				child = FileStateObj.CreateEntry(this, newPath, newRelPath);
			}
			return child;
		}

		public void Copy(string target)
		{
			switch (EntryKind) {
			case Kind.File:
				if (File.Exists(target))
					File.Delete(target);
				File.Copy(Pathname, target);

				// Keep the file attributes identical
				File.SetAttributes(target, Attributes);
				File.SetCreationTime(target, CreationTime);
				File.SetLastWriteTime(target, LastWriteTime);
				break;

			case Kind.Directory:
				CopyDirectory(target, true);
				break;

			default:
				Debug.Assert(false);
				break;
			}
		}

		public void Copy(StateEntry dirToCopyInto)
		{
			string baseName   = Path.GetFileName(Name);
			string targetPath = Path.Combine(dirToCopyInto.Pathname, baseName);

			switch (EntryKind) {
			case Kind.File:
				StateEntry other = null;
				if (! dirToCopyInto.FileStateObj.CleanFirst)
					other = dirToCopyInto.FileStateObj.FindDuplicate(this);

				if (other != null) {
					if (other.DeletePending) {
						Console.WriteLine(String.Format("optimizing by moving {0} to {1}",
														other.Pathname, targetPath));
						other.Move(targetPath);
					} else {
						Console.WriteLine(String.Format("optimizing by copying {0} to {1}",
														other.Pathname, targetPath));
						other.Copy(targetPath);
					}
				} else {
					Copy(targetPath);
				}

				dirToCopyInto.FindOrCreateChild(baseName);
				break;

			case Kind.Directory:
				CopyDirectory(targetPath, false);

				StateEntry newDir = dirToCopyInto.FindOrCreateChild(baseName);
				if (Children != null)
					foreach (StateEntry child in Children)
						child.Copy(newDir);
				break;

			default:
				Debug.Assert(false);
				break;
			}
		}

		public void CopyDirectory(string target, bool andChildren)
		{
			if (! Directory.Exists(target))
				Directory.CreateDirectory(target);

			File.SetCreationTime(target, CreationTime);
			File.SetLastWriteTime(target, LastWriteTime);

			if (andChildren && Children != null)
				foreach (StateEntry child in Children)
					child.Copy(Path.Combine(target, child.Info.Name));
		}

#if IGNORE
		public static StateEntry FindRelativeRoot(FileState fileStateObj,
												  string relativePath)
		{
			StateEntry root = fileStateObj.Root;

			string   rootPath;
			string[] relParts = relativePath.Split(Path.DirectorySeparatorChar);
			if (relParts.Length > 0)
				rootPath = relParts[0];
			else
				rootPath = relativePath;

			if (root.EntryKind == StateEntry.Kind.Collection && root.Children != null)
				foreach (StateEntry child in root.Children)
					if (Path.GetFileName(child.Pathname) == rootPath) {
						root = child;
						break;
					}

			return root;
		}

		public static StateEntry CreateDirectoryPath(FileState fileStateObj,
													 string relativePath)
		{
			return CreateDirectoryPath(FindRelativeRoot(fileStateObj, relativePath),
									   relativePath);
		}
		public static StateEntry CreateDirectoryPath(StateEntry relativeRoot,
													 string relativePath)
		{
			StateEntry entry	= relativeRoot;
			string[]   relParts = relativePath.Split(Path.DirectorySeparatorChar);

			bool first = true;
			foreach (string part in relParts) {
				if (first)
					first = false;
				else
					entry = entry.FindOrCreateChild(part);
			}
			return entry;
		}

		private static void CreateDirectoryMembers(StateEntry newBase, StateEntry reference)
		{
			foreach (StateEntry child in reference.Children) {
				StateEntry newChild =
					newBase.FindOrCreateChild(Path.GetFileName(child.Pathname));
				if (child.Children != null)
					CreateDirectoryMembers(newChild, child);
			}
		}

		public static StateEntry CreateDirectoryTree(FileState fileStateObj,
													 StateEntry reference)
		{
			StateEntry newBase = CreateDirectoryPath(fileStateObj, reference.Name);
			if (reference.Children != null)
				CreateDirectoryMembers(newBase, reference);
			return newBase;
		}			
#endif

		public void MarkDeletePending()
		{
			DeletePending = true;

			if (Children != null)
				foreach (StateEntry child in Children)
					child.MarkDeletePending();
		}

		public void Delete()
		{
			switch (EntryKind) {
			case Kind.Directory:
				if (Directory.Exists(Pathname))
					DeleteDirectory(Pathname);
				break;

			default:
				if (Info.Exists)
					Info.Delete();
				break;
			}
		}

		public static void DeleteDirectory(string path)
		{
			DirectoryInfo dirInfo = new DirectoryInfo(path);

			foreach (FileSystemInfo entry in dirInfo.GetFileSystemInfos()) {
				UnixSymbolicLinkInfo info = new UnixSymbolicLinkInfo(entry.FullName);
				if (! info.IsSymbolicLink && info.IsDirectory)
					DeleteDirectory(entry.FullName);
				else
					entry.Delete();
			}

			Directory.Delete(dirInfo.FullName);
		}

		public bool Exists()
		{
			switch (EntryKind) {
			case Kind.Directory:
				return Directory.Exists(Pathname);

			default:
				return Info.Exists;
			}
		}

		public void Move(string target)
		{
			switch (EntryKind) {
			case Kind.Directory:
				Directory.Move(Pathname, target);
				break;

			default:
				File.Move(Pathname, target);
				break;
			}
		}

		public void CompareTo(StateEntry other)
		{
			string msg;

			if (EntryKind != Kind.Collection)
				if (Info.Name != other.Info.Name) {
					msg = String.Format("{0}: name does not match {1}!!!", Pathname,
										other.Pathname);
					Console.WriteLine(msg);
					return;
				}

			string reasonChanged = null;

			if (EntryKind != other.EntryKind) {
				reasonChanged = String.Format("kind does not match: {0} != {1}",
											  EntryKind, other.EntryKind);
			}
			else if (EntryKind == Kind.File) {
				if (Checksum != other.Checksum)
					reasonChanged = "contents changed";
				else if (Length != other.Length)
					reasonChanged = String.Format("length changed ({0} != {1})",
												  Length, other.Length);
				else if (Attributes != other.Attributes)
					reasonChanged = String.Format("file attributes changed ({0} != {1})",
												  Attributes, other.Attributes);
			}

			if (reasonChanged != null)
				FileStateObj.Changes.Add(new ObjectUpdate(this, other,
														  reasonChanged));

			List<StateEntry> otherChildren = new List<StateEntry>();

			if (other.Children != null)
				foreach (StateEntry child in other.Children)
					otherChildren.Add(child);

			if (Children != null)
				foreach (StateEntry child in Children) {
					StateEntry otherChild = other.FindChild(child.Info.Name);
					if (otherChild != null) {
						otherChildren.Remove(otherChild);
						child.CompareTo(otherChild);
						continue;
					}

#if IGNORE
					StateEntry moved = other.FileStateObj.FindDuplicate(child);
					if (moved != null)
						FileStateObj.Changes.Insert(0, new ObjectMove(child, moved.Name));
					else
#endif
						FileStateObj.Changes.Add(new ObjectDelete(child));
				}

			foreach (StateEntry child in otherChildren) {
				if (FindChild(child.Info.Name) != null)
					continue;

#if IGNORE
				StateEntry moved = FileStateObj.FindDuplicate(child);
				if (moved != null)
					FileStateObj.Changes.Insert(0, new ObjectMove(moved, child.Name));
				else
#endif
					FileStateObj.Changes.Insert(0, new ObjectCopy(child, this));
			}
		}

		public void Report()
		{
			Console.WriteLine(String.Format("{0} ({1})", Pathname, Name));

			if (Children != null)
				foreach (StateEntry child in Children)
					child.Report();
		}
	}

	public abstract class StateChange {
		public abstract void Perform();
		public abstract void Report();
		public virtual  void Prepare() {}
	}

#if IGNORE
	public class ObjectMove : StateChange
	{
		StateEntry entryToMove;
		StateEntry rootToMoveTo;
		string	   relPathToMoveTo;

		public ObjectMove(StateEntry entryToMove, string relPathToMoveTo)
		{
			this.entryToMove	 = entryToMove;
			this.relPathToMoveTo = relPathToMoveTo;
			this.rootToMoveTo	 = StateEntry.FindRelativeRoot(entryToMove.FileStateObj,
															   relPathToMoveTo);
		}

		public override void Perform()
		{
			entryToMove.Parent.Children.Remove(entryToMove);

			string targetPath =
				Path.Combine(Path.GetDirectoryName(rootToMoveTo.Pathname),
							 relPathToMoveTo);

			FileInfo targetInfo = new FileInfo(targetPath);
			if (! targetInfo.Directory.Exists)
				targetInfo.Directory.Create();

			if (entryToMove.Info.Exists && ! targetInfo.Exists)
				entryToMove.Move(targetInfo.FullName);
			else if (! entryToMove.Info.Exists)
				Console.WriteLine("Attempted to move " + entryToMove.Pathname +
								  ", but it does not exist!");
			else if (targetInfo.Exists)
				Console.WriteLine("Attempted to move " + entryToMove.Pathname +
								  " to " + targetInfo.FullName +
								  ", but the latter already exists!");

			StateEntry.CreateDirectoryPath(rootToMoveTo, relPathToMoveTo);
		}

		public override void Report()
		{
			string msg = String.Format("{0}: moved to {1}",
									   entryToMove.Name, relPathToMoveTo);
			Console.WriteLine(msg);
		}
	}
#endif

	public class ObjectCopy : StateChange
	{
		StateEntry entryToCopyFrom;
		StateEntry dirToCopyTo;

		// `entryToCopyFrom' lives in the remote repository
		public ObjectCopy(StateEntry entryToCopyFrom, StateEntry dirToCopyTo)
		{
			this.entryToCopyFrom = entryToCopyFrom;
			this.dirToCopyTo	 = dirToCopyTo;
		}

		public override void Perform()
		{
			entryToCopyFrom.Copy(dirToCopyTo);
		}

		public override void Report()
		{
			string msg = String.Format("{0}: added", entryToCopyFrom.Name);
			Console.WriteLine(msg);
		}
	}

	public class ObjectUpdate : StateChange
	{
		StateEntry entryToUpdate;
		StateEntry entryToUpdateFrom;
		string	   reason;

		// `entryToUpdateFrom' lives in the remote repository
		public ObjectUpdate(StateEntry entryToUpdate, StateEntry entryToUpdateFrom,
							string reason)
		{
			this.entryToUpdate	   = entryToUpdate;
			this.entryToUpdateFrom = entryToUpdateFrom;
			this.reason			   = reason;
		}

		public override void Prepare()
		{
			entryToUpdate.FileStateObj.BackupEntry(entryToUpdate);
		}

		public override void Perform()
		{
			if (entryToUpdateFrom.Pathname != entryToUpdate.Pathname) {
				if (entryToUpdateFrom.EntryKind != entryToUpdate.EntryKind)
					entryToUpdateFrom.Copy(entryToUpdate.Parent);
				else
					entryToUpdateFrom.Copy(entryToUpdate.Pathname);
			}

			// Force the entry to update itself when the database is written
			entryToUpdate.Reset();
		}

		public override void Report()
		{
			string msg = String.Format("{0}: {1}", entryToUpdate.Name, reason);
			Console.WriteLine(msg);
		}
	}

	public class ObjectDelete : StateChange
	{
		StateEntry entryToDelete;

		public ObjectDelete(StateEntry entryToDelete)
		{
			this.entryToDelete = entryToDelete;
			entryToDelete.MarkDeletePending();
		}

		public override void Prepare()
		{
			if (entryToDelete.Exists())
				entryToDelete.FileStateObj.BackupEntry(entryToDelete);
		}

		public override void Perform()
		{
			entryToDelete.Parent.Children.Remove(entryToDelete);

			if (entryToDelete.Exists())
				entryToDelete.Delete();
		}

		public override void Report()
		{
			string msg = String.Format("{0}: removed", entryToDelete.Name);
			Console.WriteLine(msg);
		}
	}

	public class FileState
	{
		private static Regex xmlFileRe = new Regex("\\.xml$");

		public StateEntry        Root;
		public DirectoryInfo     GenerationPath;
		public bool              CleanFirst;
		public List<StateChange> Changes = new List<StateChange>();

		public Dictionary<string, StateEntry> ChecksumDict =
			new Dictionary<string, StateEntry>();

		public StateEntry FindDuplicate(StateEntry child)
		{
			StateEntry foundEntry = null;

			if (child.EntryKind == StateEntry.Kind.File && child.Checksum != null)
				if (ChecksumDict.ContainsKey(child.Checksum))
					foundEntry = ChecksumDict[child.Checksum];

			return foundEntry;
		}

		public static FileState ReadState(List<string> paths, List<Regex> IgnoreList,
										  bool verboseOutput)
		{
			FileState state = new FileState();

			state.Root = new StateEntry(state, null, null, null,
										StateEntry.Kind.Collection);

			foreach (string path in paths)
				state.GetEntryState(state.Root, path, IgnoreList, verboseOutput);

			return state;
		}
			
		public static FileState ReadState(string path, List<Regex> IgnoreList,
										  bool verboseOutput)
		{
			FileState state = new FileState();
			state.Root = state.GetEntryState(null, path, IgnoreList, verboseOutput);
			return state;
		}

		private StateEntry GetEntryState(StateEntry parent, string path,
										 List<Regex> IgnoreList, bool verboseOutput)
		{
			if (xmlFileRe.IsMatch(path)) {
				XmlDocument Document = new XmlDocument();
				Document.Load(path);
				return LoadElements(Document.FirstChild, parent);
			} else {
				return ReadEntry(parent, path, Path.GetFileName(path), IgnoreList,
								 verboseOutput);
			}
		}

		public void BackupEntry(StateEntry entry)
		{
			if (GenerationPath == null)
				return;

			FileInfo target =
				new FileInfo(Path.Combine(GenerationPath.FullName, entry.Name));
			if (! target.Directory.Exists)
				target.Directory.Create();

			entry.Copy(target.FullName);
		}

		public void CompareTo(FileState other)
		{
			Root.CompareTo(other.Root);
		}

		public void PerformChanges()
		{
			foreach (StateChange change in Changes) {
				if ((change as ObjectDelete) != null)
					change.Prepare();
			}

			if (CleanFirst)
				foreach (StateChange change in Changes) {
					if ((change as ObjectDelete) != null) {
						change.Report();
						change.Perform();
					}
				}

			foreach (StateChange change in Changes) {
				if (! CleanFirst || (change as ObjectDelete) == null) {
					change.Report();
					change.Perform();
				}
			}

			Changes = new List<StateChange>();
		}

		public void ReportChanges()
		{
			if (CleanFirst)
				foreach (StateChange change in Changes)
					if ((change as ObjectDelete) != null)
						change.Report();

			foreach (StateChange change in Changes)
				if (! CleanFirst || (change as ObjectDelete) == null)
					change.Report();
		}

		private StateEntry LoadElements(XmlNode node, StateEntry parent)
		{
			if (node.Name != "xml")
				return null;

			return StateEntry.LoadElement(this, parent, null, null,
										  node.NextSibling.FirstChild);
		}

		public void Report()
		{
			Root.Report();
		}

		public void WriteTo(XmlTextWriter w)
		{
			w.WriteStartElement("filestate");
			Root.WriteTo(w, true);
			w.WriteEndElement();
		}

		public void WriteTo(string path)
		{
			FileInfo info = new FileInfo(path);
			if (info.Exists)
				info.Delete();

			info.Directory.Create();

			FileStream fs = info.OpenWrite();
			using (StreamWriter sw = new StreamWriter(fs, Encoding.UTF8)) {
				XmlTextWriter xw = new XmlTextWriter(sw);
				xw.Formatting = Formatting.Indented;
				xw.WriteStartDocument();
				WriteTo(xw);

				sw.WriteLine();
			}
		}

		public StateEntry CreateEntry(StateEntry parent, string path,
									  string relativePath)
		{
			UnixSymbolicLinkInfo info = new UnixSymbolicLinkInfo(path);
			if (info.IsSymbolicLink) {
				return new StateEntry(this, parent, path, relativePath,
									  StateEntry.Kind.SymbolicLink);
			}
			else if (info.IsDirectory) {
				return new StateEntry(this, parent, path, relativePath,
									  StateEntry.Kind.Directory);
			}
			else if (! info.IsRegularFile) {
				return new StateEntry(this, parent, path, relativePath,
									  StateEntry.Kind.Special);
			}
			else {
				StateEntry entry = new StateEntry(this, parent, path, relativePath,
												  StateEntry.Kind.File);
				string csum = entry.Checksum;
				if (csum != null)
					ChecksumDict[csum] = entry;

				return entry;
			}
		}

		private StateEntry ReadEntry(StateEntry parent, string path, string relativePath,
									 List<Regex> IgnoreList, bool verboseOutput)
		{
			if (IgnoreList != null)
				foreach (Regex ignoreRe in IgnoreList)
					if (ignoreRe.IsMatch(path))
						return null;

			StateEntry entry = CreateEntry(parent, path, relativePath);

			if (entry.EntryKind == StateEntry.Kind.Directory)
				ReadDirectory(entry, path, relativePath, IgnoreList, verboseOutput);

			return entry;
		}

		private void ReadDirectory(StateEntry entry, string path, string relativePath,
								   List<Regex> IgnoreList, bool verboseOutput)
		{
			DirectoryInfo info = new DirectoryInfo(path);

			if (verboseOutput && relativePath.Split('/').Length < 4)
				Console.WriteLine("Summing: " + info.FullName);

			try {
				foreach (FileSystemInfo item in info.GetFileSystemInfos())
					ReadEntry(entry, item.FullName, Path.Combine(relativePath, item.Name),
							  IgnoreList, verboseOutput);
			}
			catch (Exception) {
				// Defend against access violations and failures to read!
			}
		}

		public static FileState CreateDatabase(string path, string dbfile,
											   bool report, bool verbose)
		{
			FileInfo dbfileInfo = new FileInfo(dbfile);

			if (! dbfileInfo.Directory.Exists)
				dbfileInfo.Directory.Create();

			if (dbfileInfo.Exists)
				dbfileInfo.Delete();
								
			List<Regex> ignoreList = new List<Regex>();
			ignoreList.Add(new Regex("\\.file_db\\.xml$"));

			List<string> collection = new List<string>();
			collection.Add(path);

			FileState tree = FileState.ReadState(collection, ignoreList, verbose);
			tree.WriteTo(dbfileInfo.FullName);
			if (report) {
				Console.WriteLine("created database state:");
				tree.Report();
			}

			return tree;
		}

		public static int Main(string[] args)
		{
			FileState comparator	= null;
			string	  outputFile	= null;
			string	  databaseFile	= null;
			string	  referenceDir	= null;
			bool	  updateRemote	= false;
			bool	  reportFiles	= false;
			bool	  verboseOutput = false;
			bool	  cleanFirst    = false;
			string	  generations	= null;

			List<string> realArgs	= new List<string>();
			List<Regex>	 ignoreList = new List<Regex>();

			for (int i = 0; i < args.Length; i++) {
				switch (args[i]) {
				case "-D":
					reportFiles = true;
					break;

				case "-v":
					verboseOutput = true;
					break;

				case "-C":
					cleanFirst = true;
					break;

				case "-g":
					if (i + 1 < args.Length)
						generations = args[++i];
					break;

				case "-d":
					if (i + 1 < args.Length)
						databaseFile = args[++i];
					break;

				case "-r":
					if (i + 1 < args.Length)
						referenceDir = args[++i];
					break;

				case "-u":
					updateRemote = true;
					break;

				case "-o":
					if (i + 1 < args.Length)
						outputFile = args[++i];
					break;

				case "-x":
					if (i + 1 < args.Length) {
						using (StreamReader sr = new StreamReader(args[i + 1])) {
							string line = sr.ReadLine();
							while (line != null) {
								ignoreList.Add(new Regex(line));
								line = sr.ReadLine();
							}
						}
						i++;
					}
					break;

				default:
					realArgs.Add(args[i]);
					break;
				}
			}

			if (databaseFile == null && referenceDir != null)
				databaseFile = Path.Combine(referenceDir, ".file_db.xml");

			if (databaseFile != null) {
				FileInfo dbInfo = new FileInfo(databaseFile);
				if (dbInfo.Exists) {
					comparator = FileState.ReadState(dbInfo.FullName, null, false);
					if (reportFiles) {
						Console.WriteLine("read database state:");
						comparator.Report();
					}
				}
				else if (referenceDir != null) {
					if (verboseOutput)
						Console.WriteLine("Building database in " + dbInfo.FullName + " ...");
					comparator =
						FileState.CreateDatabase(referenceDir, dbInfo.FullName,
												 reportFiles, verboseOutput);
				}
			}

			TextWriter outStream = Console.Out;
			if (outputFile != null) {
				FileInfo info = new FileInfo(outputFile);
				if (info.Exists)
					info.Delete();

				info.Directory.Create();

				FileStream fs = info.OpenWrite();
				StreamWriter sw = new StreamWriter(fs, Encoding.UTF8);
				outStream = sw;
			}

			if (realArgs.Count == 0)
				foreach (StateEntry topdir in comparator.Root.Children)
					realArgs.Add(topdir.Pathname);

			if (verboseOutput)
				Console.WriteLine("Reading files ...");
			FileState state = FileState.ReadState(realArgs, ignoreList, verboseOutput);
			if (reportFiles) {
				Console.WriteLine("read file state:");
				state.Report();
			}

			if (comparator != null) {
				if (verboseOutput)
					Console.WriteLine("Comparing details ...");
				comparator.CompareTo(state);

				if (updateRemote && comparator.Changes.Count > 0) {
					string genDir = null;
					if (updateRemote && generations != null) {
						genDir = Path.Combine(generations,
											  DateTime.Now.ToString("yyyy-MM-dd.HHmmss"));
						if (verboseOutput)
							Console.WriteLine("Backing up to generation directory: " + genDir);

						comparator.GenerationPath = new DirectoryInfo(genDir);
					}

					if (verboseOutput)
						Console.WriteLine("Updating files in " + referenceDir + " ...");
					comparator.CleanFirst = cleanFirst;
					comparator.PerformChanges();

					if (verboseOutput)
						Console.WriteLine("Updating database in " + databaseFile + " ...");
					comparator.WriteTo(databaseFile);

					if (referenceDir != null) {
						if (verboseOutput)
							Console.WriteLine("Verifying database ...");

						List<string> collection = new List<string>();
						collection.Add(referenceDir);

						List<Regex> ignore = new List<Regex>();
						ignore.Add(new Regex("\\.file_db\\.xml$"));

						state = FileState.ReadState(collection, ignore, false);

						comparator.CompareTo(state);
						comparator.ReportChanges();
					}
				}
			}
			else if (databaseFile != null) {
				state.WriteTo(databaseFile);
			}
			else {
				XmlTextWriter xw = new XmlTextWriter(outStream);
				xw.Formatting = Formatting.Indented;
				xw.WriteStartDocument();
				state.WriteTo(xw);

				outStream.WriteLine();
			}

			outStream.Close();

			return 0;
		}
	}
}
