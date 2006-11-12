bin_PROGRAMS = attic

attic_CXXFLAGS =
attic_SOURCES = \
	attic.cc binary.cc md5.c \
	DateTime.cc Path.cc FileInfo.cc Regex.cc \
	StateMap.cc StateChange.cc ChangeSet.cc \
	DataPool.cc Location.cc

if DEBUG
attic_CXXFLAGS += -DDEBUG_LEVEL=4
endif
attic_LDFLAGS = -static	# for the sake of command-line speed

#info_TEXINFOS = attic.texi

######################################################################

all-clean: maintainer-clean
	rm -fr	*~ .*~ .\#* *.html *.info *.pdf *.a *.so *.o *.lo *.la \
		*.elc *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr \
		.gdb_history gmon.out h out TAGS ledger valexpr .deps \
		.libs build AUTHORS COPYING INSTALL Makefile acconf.h \
		acconf.h.in aclocal.m4 autom4te config.guess config.sub \
		configure depcomp install-sh libtool ltconfig ltmain.sh \
		missing stamp texinfo.tex Makefile.in mkinstalldirs \
		elisp-comp elc-stamp py-compile *.gch alltests
