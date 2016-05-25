#!/bin/sh

nasm -fwin64 -o out.o out.s
if [[ "$?" == "0" ]]; then
  gcc -Wl,-einit_ -Lstdlib -o out.exe out.o -lstdlib 
fi

