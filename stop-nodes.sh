#!/bin/bash

for i in {0..9}; do
  lxc stop "node$i" &
done
wait