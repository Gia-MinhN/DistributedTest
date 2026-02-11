#!/bin/bash

for i in {0..9}; do
  lxc exec "node$i" -- pkill -x gds
done