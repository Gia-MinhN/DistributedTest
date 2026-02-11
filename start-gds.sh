#!/bin/bash

for i in {0..9}; do
  lxc exec node$i -- bash -lc "tmux kill-session -t gds 2>/dev/null || true; tmux new -d -s gds '/root/gds'"
done