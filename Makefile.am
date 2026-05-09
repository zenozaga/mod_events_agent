## Makefile.am — drop-in FreeSWITCH module build (autotools).
##
## When this directory lives at
##     freeswitch/src/mod/applications/mod_event_agent/
## the FreeSWITCH bootstrap-and-configure pipeline picks the module up
## automatically: `make` at the FS top level builds it, `make install`
## drops `mod_event_agent.so` into the FS module directory, and the
## modules.conf list activates it on the next reload.
##
## Outside the FS tree the legacy standalone Makefile (still in this
## repo) keeps working for Docker / quick-iteration flows. Both build
## paths produce the same .so; the difference is who orchestrates
## headers and link order.
##
## Two external libs are required at link time and are detected by the
## configure macros under m4/:
##   - cJSON   (PKG_CHECK_MODULES → CJSON_CFLAGS / CJSON_LIBS)
##   - libnats (PKG_CHECK_MODULES on >=3.7 → NATS_CFLAGS / NATS_LIBS,
##              with AC_CHECK_LIB fallback for older packages)
## A future driver compiled in alongside (Kafka, RabbitMQ, …) should
## follow the same conditional pattern via its own AC_DEFINE flag.

include $(top_srcdir)/build/modmake.rulesam

MODNAME = mod_event_agent
mod_LTLIBRARIES = mod_event_agent.la

mod_event_agent_la_SOURCES = \
    src/mod_event_agent.c \
    src/core/config.c \
    src/events/adapter.c \
    src/events/serializer.c \
    src/dialplan/manager.c \
    src/dialplan/commands.c \
    src/commands/handler.c \
    src/commands/core.c \
    src/commands/call.c \
    src/commands/api.c \
    src/commands/status.c \
    src/validation/validation.c \
    src/drivers/nats.c

mod_event_agent_la_CFLAGS = \
    $(AM_CFLAGS) \
    -I$(srcdir)/src \
    -I$(srcdir)/include \
    $(CJSON_CFLAGS) \
    $(NATS_CFLAGS) \
    -DWITH_NATS=1

mod_event_agent_la_LIBADD = \
    $(switch_builddir)/libfreeswitch.la \
    $(CJSON_LIBS) \
    $(NATS_LIBS)

mod_event_agent_la_LDFLAGS = \
    -avoid-version \
    -module \
    -no-undefined \
    -shared

## Optional smoke tests. Built by `make check` when libnats is on the
## host; skipped silently otherwise so the in-tree FS build never
## fails because a developer is missing the test deps.
EXTRA_DIST = \
    tests/src/show_modules_test.c \
    tests/src/test_size_limit.c \
    tests/src/test_denylist.c \
    tests/src/test_validation.c \
    tests/Makefile \
    tests/run.sh \
    INSTALL.md \
    README.md \
    autoload_configs/mod_event_agent.conf.xml
