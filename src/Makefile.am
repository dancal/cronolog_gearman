## Process this file with automake to create Makefile.in

sbin_PROGRAMS     = cronolog
sbin_SCRIPTS      = cronosplit
noinst_PROGRAMS   = cronotest

cronolog_SOURCES  = cronolog.c  cronoutils.c cronoutils.h config.h
cronotest_SOURCES = cronotest.c cronoutils.c cronoutils.h config.h

INCLUDES          = -I../lib  -I../iniparser -I../iniparser/src
LDADD             = ../lib/libutil.a  ../iniparser/libiniparser.a -lgearman -lz

EXTRA_DIST        = $(sbin_SCRIPTS) 
#mod_cronolog.c

$(PROGRAMS): $(LDADD)
