#!/usr/bin/bash

if [[ ! -e build ]]; then
    cmake -B build
fi

make -C build > /dev/null && ./build/disasm "$@"
