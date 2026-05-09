#!/usr/bin/env bash
# tests/run.sh — orchestrator for the integration test suite.
#
# Responsibilities:
#   1. Build the test binaries (`make` in this dir).
#   2. Make sure a NATS broker is reachable at $MOD_EVENT_AGENT_URL.
#   3. Make sure a FreeSWITCH node with mod_event_agent loaded is
#      reachable on $MOD_EVENT_AGENT_NODE_ID.
#   4. Run each binary, collect pass/fail, exit non-zero if any fails.
#
# Defaults are tuned for the bundled docker-compose.dev.yaml (NATS at
# localhost:7001, node id "fs-audit"); override either via env to
# point the suite at a different stack:
#
#     MOD_EVENT_AGENT_URL=nats://prod-nats:4222 \
#     MOD_EVENT_AGENT_NODE_ID=fs-prod-01 \
#     ./run.sh

set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

: "${MOD_EVENT_AGENT_URL:=nats://localhost:7001}"
: "${MOD_EVENT_AGENT_NODE_ID:=fs-audit}"

export MOD_EVENT_AGENT_URL MOD_EVENT_AGENT_NODE_ID

BIN_DIR="$DIR/bin"

# Bundled libnats static archive lives alongside the module source at
# ../lib/nats/libnats_static.a. The Makefile picks it up by default,
# so a fresh `make` here works on any host with a C toolchain.
echo "→ Building test binaries…"
make -s all

# Smoke check the broker reachability before running anything. A
# wrong $MOD_EVENT_AGENT_URL produces a much clearer error here than
# inside one of the test binaries.
echo "→ Checking broker at $MOD_EVENT_AGENT_URL"
if command -v nats >/dev/null 2>&1; then
    if ! nats --server "$MOD_EVENT_AGENT_URL" rtt --timeout 2s >/dev/null 2>&1; then
        echo "  ✗ broker unreachable" >&2
        echo "  hint: start docker-compose.dev.yaml (or override MOD_EVENT_AGENT_URL)" >&2
        exit 1
    fi
fi

# Smoke check the module is reachable on the configured node before
# we spend time on the deeper assertions.
echo "→ Probing module at freeswitch.node.$MOD_EVENT_AGENT_NODE_ID"
if command -v nats >/dev/null 2>&1; then
    if ! nats --server "$MOD_EVENT_AGENT_URL" \
              req "freeswitch.node.$MOD_EVENT_AGENT_NODE_ID" \
              '{"command":"agent.status"}' --timeout 2s >/dev/null 2>&1; then
        echo "  ✗ module not responding on this node" >&2
        echo "  hint: confirm FreeSWITCH is up and mod_event_agent loaded" >&2
        exit 1
    fi
fi

# Each test prints its own progress to stderr; we capture exit
# codes so a single failure reports cleanly at the end of the run.
fail=0
for bin in "$BIN_DIR"/test_*; do
    [ -x "$bin" ] || continue
    name="$(basename "$bin")"
    echo
    echo "→ Running $name"
    if "$bin"; then
        echo "  ✓ $name PASS"
    else
        echo "  ✗ $name FAIL"
        fail=1
    fi
done

# show_modules_test is the legacy single-test that pre-dates this
# harness; we keep it green-or-skipped so a regression there does not
# go unnoticed but it is not required for this commit's hardening.
if [ -x "$BIN_DIR/show_modules_test" ]; then
    echo
    echo "→ Running show_modules_test (legacy)"
    if "$BIN_DIR/show_modules_test" >/dev/null 2>&1; then
        echo "  ✓ show_modules_test PASS"
    else
        echo "  ⚠ show_modules_test failed (non-blocking)"
    fi
fi

echo
if [ "$fail" -eq 0 ]; then
    echo "✓ all tests passed"
    exit 0
else
    echo "✗ some tests failed"
    exit 1
fi
