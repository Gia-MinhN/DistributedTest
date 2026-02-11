#!/bin/bash

for i in {0..9}; do
  lxc start "node$i" &
done
wait