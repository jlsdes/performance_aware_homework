#!/bin/bash

exit_on_error() {
    echo "$1"
    exit
}

./run.sh $1 > temp.asm || exit_on_error "> Failed to execute run.sh successfully."
nasm temp.asm || exit_on_error "> Invalid assembly generated."
./cmp_bytes temp $1
