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

    case 'n':
      updateRemote = false;
      break;

    case 'c':
      // compute checksums...
      StateEntry::TrustMode = false;
      break;

    case 'x':
      if (i + 1 < argc) {
	ignoreList.push_back(new Regex(args[i + 1]));
	i++;
      }
      break;

    case 'X':
      if (i + 1 < argc) {
	FileInfo info(args[i + 1]);
	if (info.FileKind() == FileInfo::File) {
	  std::ifstream fin(args[i + 1]);
	  do {
	    std::string s;
	    std::getline(fin, s);
	    ignoreList.push_back(new Regex(s));
	  } while (! fin.eof() && fin.good());
	}
	i++;
      }
      break;
    }
  }

  if (realArgs.size() > 1) {
    referenceDir = realArgs.back();
    realArgs.pop_back();
  }

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
  else if (! referenceDir.empty()) {
    std::list<std::string> paths;
    paths.push_back(referenceDir);
    comparator = FileState::ReadState(paths, ignoreList, verboseOutput);
  }

  if (realArgs.empty() && comparator == NULL) {
    std::cout << "usage: fstate <OPTIONS> [-d DATABASE] [DIRECTORY ...]\n\
\n\
Where options is one or more of:\n\
  -d FILE  Specify a database to compare with\n\
  -r DIR   Specify a remote directory to reconcile against\n\
  -u       Update the given remote directory (-r)\n\
  -c       Use checksums instead of just length & timestamps This is MUCH\n\
           slower, but can optimize network traffic by discovering when files\n\
           have been moved\n\
  -b       Perform a bi-directional update between the remote directory (-r)\n\
           and the directory specified on the command-line, using the given\n\
           database (-d) as the common ancestor\n\
  -G DIR   When updating, use DIR to keep generational data\n\
  -V       Verify the database after an update is performed\n\
  -v       Be a bit more verbose\n\
  -D       Turn on debugging (be a lot more verbose)\n\
  -x REGEX Ignore all entries matching REGEX\n\
  -X FILE  Ignore all entries matching any regexp listed in FILE\n\
\n\
Here are some of the more typical forms of usage:\n\
\n\
Copy or update the directory foo to /tmp/foo:\n\
   fstate foo /tmp/foo\n\
\n\
Compare the directory foo with /tmp/foo:\n\
   fstate -n foo /tmp/foo\n\
\n\
Copy/update foo, but keep state in files.db:\n\
   fstate -d files.db foo /tmp/foo\n\
\n\
Record foo's state in a database for future checking:\n\
   fstate -d files.db foo\n\
\n\
Check foo's current state against an existing database:\n\
   fstate -n -d files.db foo\n\
This report alterations made since the database was last updated.\n\
\n\
Update the database to reflect foo's recent changes:\n\
   fstate -d files.db foo" << std::endl;
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

  }
  catch (std::exception& err) {
    std::cerr << "Error: " << err.what() << std::endl;
  }

  return 0;
}
