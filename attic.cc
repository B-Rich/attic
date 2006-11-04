#include <list>
#include <vector>
#include <map>
#include <string>
#include <exception>
#include <iostream>
#include <fstream>
#include <ctime>

#include "error.h"
#include "binary.h"
#include "Regex.h"
#include "FileInfo.h"
#include "StateChange.h"
#include "FileState.h"
#include "StateEntry.h"

#define HAVE_REALPATH
#ifdef HAVE_REALPATH
extern "C" char *realpath(const char *, char resolved_path[]);
#endif

bool DebugMode = false;

using namespace Attic;

int main(int argc, char *args[])
{
  FileState * comparator    = NULL;
  std::string outputFile;
  std::string databaseFile;
  std::string referenceDir;
  bool	      bidirectional = false;
  bool	      updateRemote  = true;
  bool	      verboseOutput = false;
  bool	      verifyData    = false;
  std::string generations;

  std::list<std::string> realArgs;
  std::list<Regex *>	 ignoreList;

  try {

  for (int i = 1; i < argc; i++) {
    if (args[i][0] != '-') {
      realArgs.push_back(File::ExpandPath(args[i]));
      continue;
    }

    switch (args[i][1]) {
    case 'b':
      bidirectional = true;
      break;

    case 'D':
      DebugMode = true;
      break;

    case 'v':
      verboseOutput = true;
      break;

    case 'V':
      verifyData = true;
      break;

    case 'G':
      if (i + 1 < argc)
	generations = File::ExpandPath(args[++i]);
      break;

    case 'd':
      if (i + 1 < argc)
	databaseFile = File::ExpandPath(args[++i]);
      break;

    case 'r':
      if (i + 1 < argc)
	referenceDir = File::ExpandPath(args[++i]);
      break;

    case 'n':
      updateRemote = false;
      break;

    case 'p':
      StateEntry::TrustMode = false;
      break;

    case 'o':
      if (i + 1 < argc)
	outputFile = File::ExpandPath(args[++i]);
      break;

    case 'x':
      if (i + 1 < argc) {
	FileInfo info(args[i + 1]);
	if (info.FileKind() == FileInfo::File) {
	  std::ifstream fin(args[i + 1]);
	  do {
	    std::string s;
	    std::getline(fin, s);
	    ignoreList.push_back(new Regex(s));
	  } while (! fin.eof() && fin.good());
	} else {
	  ignoreList.push_back(new Regex(args[i + 1]));
	}
	i++;
      }
      break;
    }
  }

  if (databaseFile.empty() && ! referenceDir.empty())
    databaseFile = Path::Combine(referenceDir, ".file_db.xml");

  bool createdDatabase = false;

  if (! databaseFile.empty()) {
    FileInfo dbInfo(databaseFile);
    if (dbInfo.Exists()) {
      std::list<Regex *> temp;
      if (verboseOutput)
	std::cout << "reading database " << databaseFile
		  << "..." << std::endl;
      comparator = FileState::ReadState(dbInfo.FullName, temp, false);
      if (comparator != NULL && DebugMode) {
	std::cout << "read database state:" << std::endl;
	comparator->Report();
      }
    }

    if (comparator == NULL && ! referenceDir.empty()) {
      if (verboseOutput)
	std::cout << "Building database in " << dbInfo.FullName
		  << " ..." << std::endl;
      comparator = FileState::CreateDatabase(referenceDir, dbInfo.FullName,
					     verboseOutput);
      createdDatabase = true;
    }
  }

  if (realArgs.empty() && comparator == NULL) {
    std::cout << "usage: fstate <OPTIONS> [-d DATABASE] [DIRECTORY ...]\n\
\n\
ere options is one or more of:\n\
-d FILE  Specify a database to compare with\n\
-r DIR   Specify a remote directory to reconcile against\n\
-u       Update the given remote directory (-r)\n\
-p       Use checksums instead of just length & timestamps\n\
         This is vastly slower, but can optimize network traffic\n\
-b       Perform a bi-directional update between the remote\n\
         directory (-r) and the directory specified on the\n\
         command-line, using the given database (-d) as the\n\
         common ancestor\n\
-G DIR   When updating, use DIR to keep generational data\n\
-V       Verify the database after an update is performed\n\
-v       Be a bit more verbose\n\
-D       Turn on debugging (be a lot more verbose)\n\
-x REGEX Ignore all entries matching REGEX\n\
-x FILE  Ignore all entries matching any regexp listed in FILE\n\
\n\
re are some of  the most typical forms of usage:\n\
\n\
Copy or update the directory foo to /tmp/foo:\n\
  fstate -u -r /tmp/foo foo\n\
\n\
Compare the directory foo to /tmp/foo:\n\
  fstate -r /tmp/foo foo\n\
\n\
Copy/update foo, but keep state in ~/db.xml:\n\
  fstate -u -d ~/db.xml -r /tmp/foo foo\n\
\n\
Record foo's state in a database for future checking:\n\
  fstate -d ~/db.xml foo\n\
\n\
Check foo's current state against an existing database:\n\
  fstate -d ~/db.xml foo\n\
This will report any alterations made since the database.\n\
\n\
Update the database to reflect foo's recent changes:\n\
  fstate -u -d ~/db.xml foo\n\
\n\
te: Typically when reconciling, fstate optimizes file copies by looking for\n\
ved files on the destination device.  If space is at a premium, however, the\n\
 flag will cause all deletions to occur first." << std::endl;
    return 1;
  }

  if (verboseOutput)
    std::cout << "Reading files ..." << std::endl;

  FileState * referent = NULL;
  FileState * state;
  if (realArgs.empty()) {
    state = comparator->Referent(verboseOutput);
  } else {
    if (bidirectional && comparator != NULL && ! referenceDir.empty())
      referent = comparator->Referent(verboseOutput);

    state = FileState::ReadState(realArgs, ignoreList, verboseOutput);
    if (DebugMode) {
      std::cout << "read file state:" << std::endl;
      state->Report();
    }
  }

  if (comparator != NULL) {
    if (verboseOutput)
      std::cout << "Comparing details ..." << std::endl;
    comparator->CompareTo(state);

    if (referent) {
      if (! comparator->Changes.empty()) {
	if (DebugMode) {
	  std::cout << "Changes between referent and state ..." << std::endl;
	  comparator->ReportChanges();
	}
      }

      comparator->CompareTo(referent, state);
      if (! state->Changes.empty()) {
	if (DebugMode) {
	  std::cout << "Changes between comparator and referent (state relative) ..."
		    << std::endl;
	  state->ReportChanges();
	}
	comparator->MergeChanges(state);
      }
    }

    if (! comparator->Changes.empty()) {
      if (updateRemote) {
	if (! referenceDir.empty()) {
	  std::string genDir;
	  if (updateRemote && ! generations.empty()) {
	    std::time_t now  = std::time(NULL);
	    std::tm *	when = std::localtime(&now);
	    char buf[64];
	    std::strftime(buf, 63, "%Y-%m-%d.%H%M%S", when);

	    genDir = Path::Combine(generations, buf);
	    if (verboseOutput)
	      std::cout << "Backing up to generation directory: " << genDir << std::endl;

	    comparator->GenerationPath = new FileInfo(genDir);
	  }

	  if (verboseOutput)
	    std::cout << "Updating files in " << referenceDir << " ..." << std::endl;
	  comparator->PerformChanges();
	}

	if (! databaseFile.empty()) {
	  if (verboseOutput)
	    std::cout << "Updating database in " << databaseFile << " ..." << std::endl;
	  comparator->WriteTo(databaseFile);
	}

	if (! referenceDir.empty() && verifyData) {
	  if (verboseOutput)
	    std::cout << "Verifying database ..." << std::endl;

	  state = FileState::CreateDatabase(referenceDir, "", verboseOutput);
	  comparator->CompareTo(state);
	  comparator->ReportChanges();
	}
      }
      else if (! createdDatabase) {
	if (verboseOutput)
	  std::cout << "Reporting changes ..." << std::endl;
	comparator->ReportChanges();
      }
    }
  }
  else if (! databaseFile.empty()) {
    if (verboseOutput)
      std::cout << "Updating database in " << databaseFile << " ..." << std::endl;
    state->WriteTo(databaseFile);
  }
  else {
    if (verboseOutput)
      std::cout << "Writing state to database files.db ..." << std::endl;
    state->WriteTo("files.db");
  }

  }
  catch (std::exception& err) {
    std::cerr << "Error: " << err.what() << std::endl;
  }

  return 0;
}
