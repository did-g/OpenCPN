#!/bin/bash
make -s
ls -l 
tail -n 1000 /home/travis/build/did-g/OpenCPN/build_s/cov-int/build-log.txt
