bin_PROGRAMS = edict edict_launcher
edict_SOURCES = edict.c picodict.c
edict_launcher_SOURCES = edict_launcher.c

AM_CFLAGS = --std=gnu99 -W -Wall

INCLUDES= $(all_includes) @EFL_CFLAGS@ @LIBKEYS_CFLAGS@ @LIBEOI_CFLAGS@ @LIBCHOICEBOX_CFLAGS@ @LIBPICODICT_CFLAGS@ @LIBLOPS_CFLAGS@

edict_LDFLAGS = $(all_libraries) @EFL_LIBS@ @LIBKEYS_LIBS@ @LIBEOI_LIBS@ @LIBCHOICEBOX_LIBS@ @LIBPICODICT_LIBS@
edict_launcher_LDFLAGS = $(all_libraries) @LIBLOPS_LIBS@
