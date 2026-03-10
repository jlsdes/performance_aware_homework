#pragma once

#include "instruction.hpp"

#include <vector>


/// Decodes a single instruction (or 2 if the first one is a prefix instruction).
Instruction decode( unsigned char const *& instruction );


/// Decodes all instructions in the given array, and prints them to the console in assembly language. The
/// original bytes are also written next to their respective assembly lines as comments. This function returns
/// a Boolean indicating whether the decoding was a success.
std::vector<Instruction> decode_all( unsigned char const * const instructions,
                                     unsigned char const * const end,
                                     bool print_instructions = false );

