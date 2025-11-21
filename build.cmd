@echo off & setlocal

mingw64\bin\g++ -std=c++23 -Wall -Wextra -Wno-unused-parameter -Wconversion -Wsign-conversion -O3 -s -static -mconsole src\main.cpp -o cbs-0.7.exe -Iinclude -ldbghelp -lbcrypt