dnl AX_LIB_NATS — detect libnats (the NATS C client) for autotools builds.
dnl
dnl   Provides:
dnl     NATS_CFLAGS / NATS_LIBS  : compile + link flags
dnl     HAVE_NATS                : automake conditional (`if HAVE_NATS`)
dnl     -DHAVE_NATS=1            : preprocessor symbol
dnl
dnl   Usage in configure.ac (right above AC_CONFIG_FILES):
dnl     AX_LIB_NATS
dnl     AS_IF([test "x$HAVE_NATS" != "xyes"], [
dnl         AC_MSG_ERROR([libnats not found — install nats.c (https://github.com/nats-io/nats.c)])
dnl     ])
dnl
dnl   The macro tries pkg-config first (libnats >= 3.7 ships nats.pc).
dnl   Older packages fall back to AC_CHECK_LIB on -lnats with a
dnl   header probe for nats/nats.h. Either path sets NATS_LIBS so the
dnl   Makefile.am can link without additional logic.

AC_DEFUN([AX_LIB_NATS], [
    HAVE_NATS=no

    PKG_CHECK_MODULES([NATS], [nats >= 3.7], [HAVE_NATS=yes], [
        AC_CHECK_HEADER([nats/nats.h], [
            AC_CHECK_LIB([nats], [natsConnection_Connect], [
                NATS_CFLAGS=""
                NATS_LIBS="-lnats"
                HAVE_NATS=yes
                AC_DEFINE([HAVE_NATS], [1], [Define if libnats is available])
            ])
        ])
    ])

    AM_CONDITIONAL([HAVE_NATS], [test "x$HAVE_NATS" = "xyes"])
    AC_SUBST([NATS_CFLAGS])
    AC_SUBST([NATS_LIBS])
])
