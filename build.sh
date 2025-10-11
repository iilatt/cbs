#!/bin/bash
set -e

g++ -std=c++23 -Wall -O3 -s src/main.cpp -o cbs-0.2 -Iinclude -Llib -ldxcompiler