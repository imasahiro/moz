#ifndef CONFIG_H_CMAKE
#define CONFIG_H_CMAKE

/* Define to 1 if you have the <stdbool.h> header file. */
#cmakedefine HAVE_STDBOOL_H 1

/* Define to 1 if you have the <db.h> header file. */
#cmakedefine HAVE_DB_H 1

/* The size of `int', as computed by sizeof. */
#cmakedefine SIZEOF_INT ${SIZEOF_INT}

/* The size of `long', as computed by sizeof. */
#cmakedefine SIZEOF_LONG ${SIZEOF_LONG}

/* The size of `void*', as computed by sizeof. */
#cmakedefine SIZEOF_VOIDP ${SIZEOF_VOIDP}

/* Define to 1 if you have the `posix_memalign' function. */
#cmakedefine HAVE_POSIX_MEMALIGN 1

/* Define to 1 if you have the `memalign' function. */
#cmakedefine HAVE_MEMALIGN 1

/* Define to 1 if you have the `__builtin_ctzl' function. */
#cmakedefine HAVE_BUILTIN_CTZL 1

/* Define to 1 if you have the `bzero' function. */
#cmakedefine HAVE_BZERO 1 

#endif /* end of include guard */
