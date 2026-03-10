cmake_minimum_required( VERSION 3.28 )
project( disasm )

set( CMAKE_CXX_STANDARD 26 )

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )
set( CMAKE_CXX_SCAN_FOR_MODULES OFF )

add_executable( disasm
    main.cpp
    decoder.cpp
    simulator.cpp
)
