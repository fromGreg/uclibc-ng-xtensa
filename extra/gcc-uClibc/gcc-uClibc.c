
/*
 * Copyright (C) 2000 Manuel Novoa III
 *
 * This is a crude wrapper to use uClibc with gcc.
 * It was originally written to work around ./configure for ext2fs-utils.
 * It certainly can be improved, but it works for me in the normal cases.
 *
 * April 7, 2001
 *
 * A bug was fixed in building the gcc command line when dynamic linking.
 * The functions dlopen, etc. now work.  At this time, you must make sure
 * the correct libdl.so is included however.  It is safest to, for example,
 * add /lib/libdl.so.1 if using ld-linux.so.1 rather than adding -ldl to the
 * command line.
 *
 * Note: This is only a problem if devel and target archs are the same.  To
 * avoid the problem, you can use a customized dynamic linker.
 *
 *
 * April 18, 2001
 *
 * The wrapper now works with either installed and uninstalled uClibc versions.
 * If you want to use the uninstalled header files and libs, either include
 * the string "build" in the invocation name such as
 *       'ln -s <ARCH>-uclibc-gcc <ARCH>-uclibc-gcc-build'
 * or include it in the environment variable setting of UCLIBC_GCC.
 * Note: This automatically enables the "rpath" behavior described below.
 *
 * The wrapper will now pass the location of the uClibc shared libs used to
 * the linker with the "-rpath" option if the invocation name includes the
 * string "rpath" or if the environment variable UCLIBC_GCC include it (as
 * with "build" above).  This is primarily intended to be used on devel
 * platforms of the same arch as the target.  A good place to use this feature
 * would be in the uClibc/test directory.
 *
 * The wrapper now displays the command line passed to gcc when '-v' is used.
 *
 */

/*
 *
 * TODO:
 * Check/modify gcc-specific environment variables?
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "gcc-uClibc.h"

static char *rpath_link[] = {
	"-Wl,-rpath-link,"UCLIBC_INSTALL_DIR"lib",
	"-Wl,-rpath-link,"UCLIBC_BUILD_DIR
};

static char *rpath[] = {
	"-Wl,-rpath,"UCLIBC_INSTALL_DIR"lib",
	"-Wl,-rpath,"UCLIBC_BUILD_DIR
};

static char *uClibc_inc[] = {
	"-I"UCLIBC_INSTALL_DIR"include/",
	"-I"UCLIBC_BUILD_DIR"include/"
};

static char *crt0_path[] = {
	UCLIBC_INSTALL_DIR"lib/crt0.o",
	UCLIBC_BUILD_DIR"crt0.o"
};

static char *lib_path[] = {
	"-L"UCLIBC_INSTALL_DIR"lib",
	"-L"UCLIBC_BUILD_DIR
};

static char static_linking[] = "-static";
static char nostdinc[] = "-nostdinc";
static char nostartfiles[] = "-nostartfiles";
static char nodefaultlibs[] = "-nodefaultlibs";
static char nostdlib[] = "-nostdlib";

int main(int argc, char **argv)
{
	int linking = 1, use_static_linking = 0;
	int use_stdinc = 1, use_start = 1, use_stdlib = 1;
	int source_count = 0, use_rpath = 0, verbose = 0;
	int i, j;
	char ** gcc_argv;
	char *ep;

	ep = getenv("UCLIBC_GCC");
	if (!ep) {
		ep = "";
	}

	if ((strstr(argv[0],"rpath") != 0) || (strstr(ep,"rpath") != 0)) {
		use_rpath = 1;
	}

#if 0
	/* Erik added this stuff in.  Disabled but kept in case the new changes */
	/* don't do what he needed. */

	/* FIXME: We need to work out the install vs use-in-built-dir
	 * issue..*/
	/* Apparently gcc doesn't accept this stuff via the command line */
	setenv("COMPILER_PATH", UCLIBC_DIR"extra/gcc-uClibc/", 1);
	setenv("LIBRARY_PATH", UCLIBC_DIR"lib/", 1);
	/* The double '/' works around a gcc bug */
	setenv("GCC_EXEC_PREFIX", UCLIBC_DIR"extra/gcc-uClibc//", 1); 
#endif

	for ( i = 1 ; i < argc ; i++ ) {
		if (argv[i][0] == '-') { /* option */
			switch (argv[i][1]) {
				case 'c':		/* compile or assemble */
				case 'S':		/* generate assembler code */
				case 'E':		/* preprocess only */
				case 'r':		/* partial-link */
				case 'i':		/* partial-link */
				case 'M':       /* map file generation */
					if (argv[i][2] == 0) linking = 0;
					break;
				case 'v':		/* verbose */
					if (argv[i][2] == 0) verbose = 1;
					break;
				case 'n':
					if (strcmp(nostdinc,argv[i]) == 0) {
						use_stdinc = 0;
					} else if (strcmp(nostartfiles,argv[i]) == 0) {
						use_start = 0;
					} else if (strcmp(nodefaultlibs,argv[i]) == 0) {
						use_stdlib = 0;
					} else if (strcmp(nostdlib,argv[i]) == 0) {
						use_start = 0;
						use_stdlib = 0;
					}
					break;
				case 's':
					if (strcmp(static_linking,argv[i]) == 0) {
						use_static_linking = 1;
					}
					break;
			    case 'W':		/* -static could be passed directly to ld */
					if (strncmp("-Wl,",argv[i],4) == 0) {
						if (strstr(argv[i],static_linking) != 0) {
							use_static_linking = 1;
						}
					}
					break;
				case '-':
					if (strcmp(static_linking,argv[i]+1) == 0) {
						use_static_linking = 1;
					}
					break;
			}
		} else {				/* assume it is an existing source file */
			++source_count;
		}
	}

#if 1
	gcc_argv = __builtin_alloca(sizeof(char*) * (argc + 20));
#else
	if (!(gcc_argv = malloc(sizeof(char) * (argc + 20)))) {
		return EXIT_FAILURE;
	}
#endif

	i = 0;
	gcc_argv[i++] = GCC_BIN;
	for ( j = 1 ; j < argc ; j++ ) {
		gcc_argv[i++] = argv[j];
	}
	if (use_stdinc) {
		gcc_argv[i++] = nostdinc;
		gcc_argv[i++] = uClibc_inc[0];
		gcc_argv[i++] = uClibc_inc[1];
		gcc_argv[i++] = GCC_INCDIR;
	}
	if (linking && source_count) {
		if (!use_static_linking) {
			if (DYNAMIC_LINKER[0]) { /* not empty string */
				gcc_argv[i++] = "-Wl,--dynamic-linker,"DYNAMIC_LINKER;
				if (strstr(DYNAMIC_LINKER,"uclibc") != 0) {	/* custom linker */
					use_rpath = 0; /* so -rpath not needed for normal case */
				}
			}
			if (use_rpath) {
				gcc_argv[i++] = rpath[0];
				gcc_argv[i++] = rpath[1];
			}
		}
		gcc_argv[i++] = rpath_link[0]; /* just to be safe */
		gcc_argv[i++] = rpath_link[1]; /* just to be safe */
		gcc_argv[i++] = lib_path[0];
		gcc_argv[i++] = lib_path[1];
		if (use_start) {
			struct stat buf;
			if (stat(crt0_path[0], &buf) >= 0) {
				gcc_argv[i++] = crt0_path[0];
			} else {
				gcc_argv[i++] = crt0_path[1];
			}
		}
		if (use_stdlib) {
			gcc_argv[i++] = nostdlib;
			gcc_argv[i++] = "-lc";
			gcc_argv[i++] = GCC_LIB;
		}
	}
	gcc_argv[i++] = NULL;

	if (verbose) {
		printf("Invoked as %s\n", argv[0]);
		for ( j = 0 ; gcc_argv[j] ; j++ ) {
			printf("arg[%2i] = %s\n", j, gcc_argv[j]);
		}
	}

	return execvp(GCC_BIN, gcc_argv);
}
