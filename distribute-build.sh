#!/bin/bash

for i in {0..9}; do
  lxc file push -p ./gds "node$i$REMOTE_DIR/root/gds" &
done
wait