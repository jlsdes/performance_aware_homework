#!/usr/bin/bash

make -C build > /dev/null && ./disasm "$@"
