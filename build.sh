#!/bin/bash
set -e

/opt/xpack-gcc-15.2.0-1/bin/g++ -std=c++23 -DCBS_LINUX -Wall -Wextra -Wno-unused-parameter -Wconversion -Wsign-conversion -O3 -s -static src/main.cpp -o cbs-0.9 -Iinclude