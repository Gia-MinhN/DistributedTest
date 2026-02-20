#!/bin/bash

CONTAINER="node0"
REMOTE_DIR="/root/gds_build"
OUT_BIN_LOCAL="./gds"

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

echo "[3/5] Compiling in container"
lxc exec "$CONTAINER" -- bash -lc "
  set -e
  if ! command -v g++ >/dev/null || ! command -v make >/dev/null; then
    apt-get update -y >/dev/null
    DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential make >/dev/null
  fi
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
