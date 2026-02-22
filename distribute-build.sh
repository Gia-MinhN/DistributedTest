#!/bin/bash

for i in {0..9}; do
  lxc file push -p ./gds "node$i$REMOTE_DIR/root/gds" &
  lxc file push -p ./seeds.conf "node$i$REMOTE_DIR/root/seeds.conf" &
done
wait