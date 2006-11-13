pkgconfigdir=$(libdir)/pkgconfig
libincludedir=$(includedir)/libburn

lib_LTLIBRARIES = libburn/libburn.la libisofs/libisofs.la

## ========================================================================= ##

# Build libraries
libburn_libburn_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
libburn_libburn_la_LIBADD = $(LIBBURN_ARCH_LIBS) $(THREAD_LIBS)
libburn_libburn_la_SOURCES = \
	libburn/async.c \
	libburn/async.h \
	libburn/back_hacks.h \
	libburn/cleanup.c \
	libburn/cleanup.h \
	libburn/crc.c \
	libburn/crc.h \
	libburn/debug.c \
	libburn/debug.h \
	libburn/drive.c \
	libburn/drive.h \
	libburn/error.h \
	libburn/file.c \
	libburn/file.h \
	libburn/init.c \
	libburn/init.h \
	libburn/lec.c \
	libburn/lec.h \
	libburn/libburn.h \
	libburn/libdax_audioxtr.h \
	libburn/libdax_audioxtr.c \
	libburn/libdax_msgs.h \
	libburn/libdax_msgs.c \
	libburn/mmc.c \
	libburn/mmc.h \
	libburn/null.c \
	libburn/null.h \
	libburn/options.c \
	libburn/options.h \
	libburn/read.c \
	libburn/read.h \
	libburn/sbc.c \
	libburn/sbc.h \
	libburn/sector.c \
	libburn/sector.h \
	libburn/sg.c \
	libburn/sg.h \
	libburn/source.h \
	libburn/source.c \
	libburn/spc.c \
	libburn/spc.h \
	libburn/structure.c \
	libburn/structure.h \
	libburn/toc.c \
	libburn/toc.h \
	libburn/transport.h \
	libburn/util.c \
	libburn/util.h \
	libburn/write.c \
	libburn/write.h \
	version.h

##	libburn/sg-@ARCH@.c \

libisofs_libisofs_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
libisofs_libisofs_la_SOURCES = \
	libisofs/tree.h \
	libisofs/tree.c \
	libisofs/volume.h \
	libisofs/volume.c \
	libisofs/util.h \
	libisofs/util.c \
	libisofs/ecma119.c \
	libisofs/ecma119.h \
	libisofs/ecma119_tree.c \
	libisofs/ecma119_tree.h \
	libisofs/susp.h \
	libisofs/susp.c \
	libisofs/rockridge.h \
	libisofs/rockridge.c \
	libisofs/joliet.c \
	libisofs/joliet.h \
	libisofs/exclude.c \
	libisofs/exclude.h \
	libisofs/hash.h \
	libisofs/hash.c 

libinclude_HEADERS = \
	libburn/libburn.h \
	libisofs/libisofs.h

## ========================================================================= ##

## Build test applications
noinst_PROGRAMS = \
	test/libburner \
	test/dewav \
	test/fake_au \
	test/iso \
	test/poll \
	test/toc \
	test/structest

bin_PROGRAMS = \
	cdrskin/cdrskin

test_libburner_CPPFLAGS = -Ilibburn
test_libburner_LDADD = $(libburn_libburn_la_OBJECTS) $(THREAD_LIBS)
test_libburner_SOURCES = test/libburner.c
test_dewav_CPPFLAGS = -Ilibburn
test_dewav_LDADD = $(libburn_libburn_la_OBJECTS) $(THREAD_LIBS)
test_dewav_SOURCES = test/dewav.c
test_fake_au_CPPFLAGS = 
test_fake_au_LDADD =
test_fake_au_SOURCES = test/fake_au.c
test_poll_CPPFLAGS = -Ilibburn
test_poll_LDADD = $(libburn_libburn_la_OBJECTS) $(THREAD_LIBS)
test_poll_SOURCES = test/poll.c
test_toc_CPPFLAGS = -Ilibburn
test_toc_LDADD = $(libburn_libburn_la_OBJECTS) $(THREAD_LIBS)
test_toc_SOURCES = test/toc.c
test_structest_CPPFLAGS = -Ilibburn
test_structest_LDADD = $(libburn_libburn_la_OBJECTS) $(THREAD_LIBS)
test_structest_SOURCES = test/structest.c
test_iso_CPPFLAGS = -Ilibisofs
test_iso_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
test_iso_SOURCES = test/iso.c

##  cdrskin construction site - ts A60816
cdrskin_cdrskin_CPPFLAGS = -Ilibburn
cdrskin_cdrskin_CFLAGS = -DCdrskin_libburn_0_2_3
cdrskin_cdrskin_LDADD = $(libburn_libburn_la_OBJECTS) $(THREAD_LIBS)
cdrskin_cdrskin_SOURCES = cdrskin/cdrskin.c cdrskin/cdrfifo.c cdrskin/cdrfifo.h cdrskin/cdrskin_timestamp.h
## cdrskin_cdrskin_SOURCES = cdrskin/cdrskin.c cdrskin/cdrfifo.c cdrskin/cdrfifo.h cdrskin/cleanup.c cdrskin/cleanup.h cdrskin/cdrskin_timestamp.h
##
## Open questions: how to compute $timestamp and express -DX="$timestamp"
##                 



## ========================================================================= ##

## Build documentation (You need Doxygen for this to work)
webhost = http://libburn-api.pykix.org
webpath = /
docdir = $(DESTDIR)$(prefix)/share/doc/$(PACKAGE)-$(VERSION)

doc:	doc/html

doc/html: doc/doxygen.conf
	if [ -f ./doc/doc.lock ]; then \
		$(RM) -r doc/html; \
		doxygen doc/doxygen.conf; \
	fi

doc-upload: doc/html
	scp -r $</* $(webhost):$(webpath)

all: doc

install-data-local: 
	if [ -f ./doc/doc.lock ]; then \
		$(mkinstalldirs) $(docdir)/html; \
		$(INSTALL_DATA) doc/html/* $(docdir)/html; \
	fi

uninstall-local:
	rm -rf $(docdir)

## ========================================================================= ##

# Indent source files
indent_files = \
	$(libisofs_libisofs_la_SOURCES) \
	$(libburn_libburn_la_SOURCES) \
	$(test_libburner_SOURCES) \
	$(test_poll_SOURCES) \
	$(test_toc_SOURCES) \
	$(test_structest_SOURCES) \
	$(test_iso_SOURCES)
	

indent: $(indent_files)
	indent -bad -bap -nbbb -nbbo -nbc -bli0 -br -bls   \
		-cdw -ce -cli0 -ncs -nbfda -i8 -l79 -lc79   \
		-lp -saf -sai -nprs -npsl -saw -sob -ss -ut \
		-sbi0 -nsc -ts8 -npcs -ncdb -fca            \
		$^

.PHONY: indent

## ========================================================================= ##

# Extra things
nodist_pkgconfig_DATA = \
	libburn-1.pc \
	libisofs-1.pc

EXTRA_DIST = \
	libburn-1.pc.in \
	libisofs-1.pc.in \
	version.h.in \
	doc/comments \
	doc/doxygen.conf.in \
	README \
	AUTHORS \
	CONTRIBUTORS \
	COPYRIGHT \
	cdrskin/README \
	cdrskin/cdrecord_spy.sh \
	cdrskin/compile_cdrskin.sh \
	cdrskin/changelog.txt \
	cdrskin/cdrskin_eng.html \
	cdrskin/wiki_plain.txt \
	cdrskin/cleanup.h \
	cdrskin/cleanup.c \
	libburn/sg-freebsd.c \
	libburn/sg-linux.c \
	COPYING

