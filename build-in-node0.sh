#!/bin/bash
set -euo pipefail

CONTAINER="node0"
REMOTE_DIR="/root/gds_build"
OUT_BIN_LOCAL="./gds"

# Sanity checks
if [[ ! -d "src" ]]; then
  echo "/src directory not found."
  exit 1
fi
if [[ ! -f "Makefile" ]]; then
  echo "Makefile not found."
  exit 1
fi

echo "[1/5] Creating build dir in $CONTAINER:$REMOTE_DIR"
lxc exec "$CONTAINER" -- bash -lc "rm -rf '$REMOTE_DIR' && mkdir -p '$REMOTE_DIR/src'"

echo "[2/5] Copying source files to container"
lxc file push -p ./Makefile "$CONTAINER$REMOTE_DIR/Makefile"
lxc file push -p -r ./src "$CONTAINER$REMOTE_DIR/"

echo "[3/5] Installing build tools (if needed) and compiling in container"
lxc exec "$CONTAINER" -- bash -lc "
  set -e
  apt-get update -y >/dev/null
  DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential make >/dev/null
  cd '$REMOTE_DIR'
  make clean >/dev/null 2>&1 || true
  make
"

echo "[4/5] Pulling binary back to VM"
lxc file pull "$CONTAINER$REMOTE_DIR/gds" "$OUT_BIN_LOCAL"
chmod +x "$OUT_BIN_LOCAL"

echo "[5/5] Cleaning up build dir in VM"
lxc exec "$CONTAINER" -- bash -lc "rm -rf '$REMOTE_DIR'"

echo "Done. Built binary saved to: $OUT_BIN_LOCAL"
