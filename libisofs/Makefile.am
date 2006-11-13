pkgconfigdir=$(libdir)/pkgconfig
libincludedir=$(includedir)/libburn

##bin_PROGRAMS = test

lib_LTLIBRARIES = libisofs.la

libisofs_la_SOURCES = \
	tree.h \
	tree.c \
	volume.h \
	volume.c \
	util.h \
	util.c \
	ecma119.c \
	ecma119.h \
	ecma119_tree.c \
	ecma119_tree.h \
	susp.h \
	susp.c \
	rockridge.h \
	rockridge.c \
	joliet.c \
	joliet.h 

libinclude_HEADERS = libisofs.h

##test_SOURCES = test.c
##test_LDADD = libisofs.la

##noinst_PROGRAMS = test
##test_SOURCES = test.c
##test_LDADD = $(libisofs_la_OBJECTS)

##INCLUDES = -I../burn/libburn

## ========================================================================= ##
indent_files = $(libisofs_la_SOURCES) 

indent: $(indent_files)
	indent -bad -bap -nbbb -nbbo -nbc -bli0 -br -bls   \
	       -cdw -ce -cli0 -ncs -nbfda -i8 -l79 -lc79   \
	       -lp -saf -sai -nprs -npsl -saw -sob -ss -ut \
	       -sbi0 -nsc -ts8 -npcs -ncdb -fca            \
	       $^

.PHONY: indent

## ========================================================================= ##
