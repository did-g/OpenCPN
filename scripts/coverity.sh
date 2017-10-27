#!/bin/bash
make -s
ls -l 
head -n 7000 /home/travis/build/did-g/OpenCPN/build_s/cov-int/build-log.txt
