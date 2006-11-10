#include "DataPool.h"

bool DebugMode = false;

using namespace Attic;

int main(int argc, char *args[])
{
  try {

  DataPool pool;
  Location optionTemplate;

  for (int i = 1; i < argc; i++) {
    if (args[i][0] != '-') {
      pool.Locations.push_back(new Location(Path::ExpandPath(args[i])));
      continue;
    }

    switch (args[i][1]) {
    case 'b':
      optionTemplate.PreserveChanges = true;
      break;

    case '>': {
      Path database(Path::ExpandPath(args[++i]));
      if (! File::Exists(database))
	return 1;

      std::cout << "Dumping state database '" << database << "':"
		<< std::endl;

      StateMap stateMap;
      stateMap.LoadFrom(database);
      if (stateMap.Root)
	stateMap.Root->DumpTo(std::cout);
      else
	std::cout << "Database has no contents!" << std::endl;

      return 0;
    }

    case 'D':
      DebugMode = true;
      break;

    case 'v':
      optionTemplate.VerboseLogging = true;
      break;

    case 'V':
      optionTemplate.VerifyResults = true;
      break;

#if 0
    case 'G':
      if (i + 1 < argc)
	generations = Path::ExpandPath(args[++i]);
      break;
#endif

    case 'd':
      if (i + 1 < argc)
	pool.LoadAncestorFromFile(Path::ExpandPath(args[++i]));
      break;

    case 'n':
      pool.LoggingOnly = true;
      break;

    case 'c':
      // compute checksums...
      optionTemplate.TrustTimestamps = false;
      break;

    case 'x':
      if (i + 1 < argc) {
	optionTemplate.Regexps.push_back(new Regex(args[i + 1]));
	i++;
      }
      break;

    case 'X':
      if (i + 1 < argc) {
	FileInfo info(args[i + 1]);
	if (info.IsRegularFile()) {
	  std::ifstream fin(Path::ExpandPath(args[i + 1]).c_str());
	  do {
	    std::string s;
	    std::getline(fin, s);
	    optionTemplate.Regexps.push_back(new Regex(s));
	  } while (! fin.eof() && fin.good());
	}
	i++;
      }
      break;
    }
  }

  if (pool.Locations.empty()) {
    std::cout << "usage: attic <OPTIONS> [DIRECTORY ...]\n\
\n\
Options accepted:\n\
    -d FILE   Specify the database containing the common ancestor\n\
    -r DIR    Specify a remote directory to reconcile against\n\
    -u        Update the given remote directory (-r)\n\
    -c        Use checksums instead of just length & timestamps.\n\
              This is MUCH slower, but can optimize network traffic\n\
              by discovering when files have been moved\n\
    -b        Perform a bi-directional update among all directories\n\
              specified on the command-line, using the given\n\
              database (-d) as the common ancestor\n\
    -G DIR    When updating, use DIR to keep generational data\n\
    -V        Verify the database after an update is performed\n\
    -v        Be a bit more verbose\n\
    -D        Turn on debugging (be a lot more verbose)\n\
    -x REGEX  Ignore all entries matching REGEX\n\
    -X FILE   Ignore all entries matching any regexp listed in FILE\n\
\n\
Here are some of the more typical forms of usage:\n\
\n\
Copy or update the directory foo to /tmp/foo:\n\
    attic foo /tmp/foo\n\
\n\
Compare the directory foo with /tmp/foo:\n\
    attic -n foo /tmp/foo\n\
\n\
Copy/update foo, but keep state in files.db:\n\
    attic -d files.db foo /tmp/foo\n\
\n\
Record foo's state in a database for future checking:\n\
    attic -d files.db foo\n\
\n\
Check foo's current state against an existing database:\n\
    attic -n -d files.db foo\n\
This report alterations made since the database was last updated.\n\
\n\
Update the database to reflect foo's recent changes:\n\
    attic -d files.db foo" << std::endl;
    return 1;
  }

  for (std::vector<Location *>::iterator i = pool.Locations.begin();
       i != pool.Locations.end();
       i++)
    (*i)->CopyOptions(optionTemplate);
  
  pool.Locations.front()->PreserveChanges = true;
  pool.UseAncestor();

  pool.ComputeChanges();
  pool.ApplyChanges(std::cout);

#if 0
  bool createdDatabase = false;

  StateMap * referent = NULL;
  StateMap * state;
  if (realArgs.empty()) {
    state = comparator->Referent(verboseOutput);
  } else {
    if (bidirectional && comparator != NULL && ! referenceDir.empty())
      referent = comparator->Referent(verboseOutput);

    state = StateMap::ReadState(realArgs, ignoreList, verboseOutput);
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
#if 0
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
#endif

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

	  state = StateMap::CreateDatabase(referenceDir, "", verboseOutput);
	  comparator->CompareTo(state);
	  if (! comparator->Changes.empty()) {
	    comparator->ReportChanges();
	    std::cout << "ERROR!" << std::endl;
	  }
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
      std::cout << "Writing state to database ./files.db ..." << std::endl;
    state->WriteTo("files.db");
  }
#endif

  }
  catch (std::exception& err) {
    std::cerr << "Error: " << err.what() << std::endl;
  }

  return 0;
}
