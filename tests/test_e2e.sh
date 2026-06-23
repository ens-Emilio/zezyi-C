#!/bin/bash
# End-to-end test: start server, verify health, run all unit tests
# Uses port 18080 to avoid conflicts with any existing service on 8080.

set -e

PORT=18080
SERVER_PID=""

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "=== E2E Test ==="

# Build server
cd server
go build -o zezyi-server
PORT=$PORT ./zezyi-server &
SERVER_PID=$!
cd ..

# Wait for server to start
sleep 1

# Verify server is actually up (retry a few times)
for i in 1 2 3 4 5; do
    if curl -s "http://localhost:$PORT/health" >/dev/null 2>&1; then
        break
    fi
    sleep 1
done

# Test health endpoint
HEALTH=$(curl -s "http://localhost:$PORT/health")
if [ "$HEALTH" != "ok" ]; then
    echo "FAIL: health endpoint returned '$HEALTH'"
    exit 1
fi
echo "PASS: health endpoint"

# Test unit tests
make test
make test_determinism
make test_replay
echo "PASS: all unit tests"

echo "=== E2E Complete ==="
