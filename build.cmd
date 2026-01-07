@echo off & setlocal

mingw64\bin\g++ -std=c++23 -DCBS_WIN32 -Wall -Wextra -Wno-unused-parameter -Wconversion -Wsign-conversion -O3 -s -static -mconsole src\main.cpp -o cbs-0.9.exe -Iinclude -ldbghelp -lbcrypt