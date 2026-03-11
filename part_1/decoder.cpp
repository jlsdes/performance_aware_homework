#include "decoder.hpp"
#include "instruction.hpp"

#include <format>
#include <functional>
#include <print>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>


/// Converts 1 or 2 bytes into an integer, and advances the instruction pointer beyond the converted bytes.
/// 'w' determines the number of bytes (false: 1, true: 2), and 's' determines whether a signed (true) or
/// unsigned (false) value is read. The returned value is a signed integer either way.
inline int get_value( unsigned char const *& instruction, bool const w, bool const s ) {
    unsigned char const * const value { instruction };
    instruction += 1 + w;
    int result;
    switch ( w << 1 | s ) {
    case 0b00:
        return *value;
    case 0b01:
        return *reinterpret_cast<char const *>( value );
    case 0b10:
        return *reinterpret_cast<unsigned short const *>( value );
    case 0b11:
        return *reinterpret_cast<short const *>( value );
    default:
        throw std::runtime_error( "This should not happen..." );
    }
}

inline Immediate get_immediate( unsigned char const *& instruction, bool const w, bool const s ) {
    return { get_value( instruction, w, s), w };
}


/// Returns the register name or memory location targetted by the instruction parameters.
/// This function expects the 'instruction' argument to have been advanced past the 2 instruction header bytes,
/// and will advance the pointer further if it needs to read a displacement value.
std::string get_r_m_name( unsigned char const r_m,
                          unsigned char const mod,
                          unsigned char const w,
                          unsigned char const *& instruction ) {
    if ( mod == 0b11 ) // r_m represents a register
        return reg_names[(w << 3) | r_m];

    bool const direct_address { mod == 0b00 and r_m == 0b110 };
    bool const no_displacement { mod == 0b00 and r_m != 0b110 };
    bool const wide_displacement { mod == 0b10 or direct_address };
    int const displacement { no_displacement ? 0 : get_value( instruction, wide_displacement, true ) };

    if ( direct_address )
        return std::format( "[{}]", displacement );
    else if ( displacement )
        return std::format( "[{} {} {}]", reg_sums[r_m], (displacement < 0 ? '-' : '+'), std::abs( displacement ) );
    else
        return std::format( "[{}]", reg_sums[r_m] );
};


Operand get_r_m( unsigned char const r_m,
                 unsigned char const mod,
                 unsigned char const w,
                 unsigned char const *& instruction ) {
    if ( mod == 0b11 ) // r_m represents a register
        return Register { r_m, w };

    bool const is_direct_address { mod == 0b00 and r_m == 0b110 };
    bool const no_displacement { mod == 0b00 and r_m != 0b110 };
    bool const wide_displacement { mod == 0b10 or is_direct_address };
    int const displacement { no_displacement ? 0 : get_value( instruction, wide_displacement, true ) };

    if ( is_direct_address )
        return direct_address( displacement );

    EffectiveAddress result { address_sums[r_m] };
    result.offset = displacement;
    return result;
};


/// Decodes instructions that have the structure defined in the 'Instruction' struc below. These operate on either
/// two registers, or one register and a memory location.
/// The 'force_swap' attribute forces the 'd' bit to be ignored, and to always swap the source and destination.
/// This is used for the out instruction (e.g.?). The 'force_wide' attribute similarly, forces the 'w' bit to be
/// ignored, and to always operate using 16 bit values.
struct RmToRegDecoder {
    bool force_swap { false };
    bool force_wide { false };

    Instruction operator()( unsigned char const *& instruction ) {
        struct HeaderBytes {
            // First byte
            unsigned char w : 1;
            unsigned char d : 1;
            unsigned char opcode : 6;
            // Second byte
            unsigned char r_m : 3;
            unsigned char reg : 3;
            unsigned char mod : 2;
        };
        auto const values { reinterpret_cast<HeaderBytes const *>( instruction ) };

        bool const wide { force_wide or values->w };
        bool const swap { force_swap or values->d };

        Instruction result { instruction };
        instruction += 2;
        result.operands[swap ? 1 : 0] = get_r_m( values->r_m, values->mod, values->w, instruction );
        result.operands[swap ? 0 : 1] = Register { values->reg, wide, false };

        return result;
    }
};


/// Decodes instructions that have the structure defined in the 'Instruction' struct below. These operate on
/// an immediate value and a register or memory location.
/// The 'mid_type' attribute indicates whether the type keywords "word" and "byte" should be placed in the
/// middle of the instructions (e.g. mov [bp + di], byte 7) rather than at the start (e.g. add byte [bx], 34).
struct ImmToRmDecoder {
    bool is_move { false };

    Instruction operator()( unsigned char const *& instruction ) {
        struct HeaderBytes {
            // First byte
            unsigned char w : 1;
            unsigned char s : 1;
            unsigned char opcode : 6;
            // Second byte
            unsigned char r_m : 3;
            unsigned char subop : 3;
            unsigned char mod : 2;
        };
        auto const values { reinterpret_cast<HeaderBytes const *>( instruction ) };

        bool const wide { (is_move or values->s == 0) and values->w == 1 };

        Instruction result { instruction };
        instruction += 2;
        result.operands[0] = get_r_m( values->r_m, values->mod, values->w, instruction );
        result.operands[1] = get_immediate( instruction, wide, values->s );
        result.write_size = values->mod != 0b11;

        return result;
    }
};


/// Decodes instructions that have the structure defined in the 'Instruction' struct below. These operate on
/// an accumulator register and a register or memory location.
/// The 'address' attribute indicates whether the following byte(s) contain a memory location or an actual
/// immediate value.
struct ImmToAccumDecoder {
    bool address { false };

    Instruction operator()( unsigned char const *& instruction ) {
        struct HeaderByte {
            unsigned char w : 1;
            unsigned char d : 1;
            unsigned char opcode : 6;
        };
        auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

        int const op_a { header->d ? 1 : 0 };

        Instruction result { instruction };
        ++instruction;
        int const value { get_value( instruction, header->w, true ) };

        result.operands[header->d ? 1 : 0] = Register { RegAX, header->w };
        result.operands[header->d ? 0 : 1] = address ? Operand { direct_address( value ) }
                                                     : Operand { Immediate ( value, header->w ) };

        return result;
    }
};


/// Decodes instructions that have the structure defined in the 'Instruction' struct below. These operate on
/// single byte instructions that have the register ID within that first byte.
/// The 'operand' attribute can be used to add a static operand between the mnemonic and the register name.
struct SingleRegDecoder {
    bool has_operand { false };
    Register operand {};

    Instruction operator()( unsigned char const *& instruction ) {
        struct HeaderByte {
            unsigned char reg : 3;
            unsigned char opcode : 5;
        };
        auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

        Instruction result { instruction };
        ++instruction;

        if ( has_operand )
            result.operands[0] = operand;
        result.operands[1] = Register { header->reg, 1 };

        return result;
    };
};


/// Decodes simple instructions that either have no operand, or just one value in the following byte(s).
struct MnemonicDecoder {
    unsigned int header_bytes { 1 };
    bool has_value { false };
    bool wide { true };
    bool sign { true };

    Instruction operator()( unsigned char const *& instruction ) {
        Instruction result { instruction };
        instruction += header_bytes;

        if ( has_value )
            result.operands[0] = get_immediate( instruction, wide, sign);

        return result;
    };
};


/// Decodes move instructions that simply move an immediate value into a register.
Instruction move_immediate_reg( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char reg : 3;
        unsigned char w : 1;
        unsigned char opcode : 4;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    Instruction result { instruction };
    ++instruction;
    result.operands[0] = Register { header->reg, header->w };
    result.operands[1] = get_immediate( instruction, header->w, true );
    return result;
}


/// Decodes move instructions that use a segment register.
Instruction move_r_m_seg( unsigned char const *& instruction ) {
    struct HeaderBytes {
        // First byte
        unsigned char zero : 1;
        unsigned char d : 1;
        unsigned char opcode : 6;
        // Second byte
        unsigned char r_m : 3;
        unsigned char seg_reg : 2;
        unsigned char zero2 : 1;
        unsigned char mod : 2;
    };
    auto const header { reinterpret_cast<HeaderBytes const *>( instruction ) };

    Instruction result { instruction };
    instruction += 2;
    result.operands[header->d ? 1 : 0] = get_r_m( header->r_m, header->mod, true, instruction );
    result.operands[header->d ? 0 : 1] = SegmentRegister { header->seg_reg };
    return result;
}
 

/// Decodes conditional jump instructions. The offset values are relative to the end of the instruction in
/// machine code, but to the start of the instruction in assembly, so we still need to add 2 to get correct
/// offsets. (All instructions that are decoded by this function are exactly 2 bytes large.)
/// Also, there is a post-processing step where these offsets are converted into labels if possible.
Instruction jump_conditional( unsigned char const *& instruction ) {
    Instruction result { instruction };
    ++instruction;
    int const destination { get_value( instruction, false, true ) };
    result.operands[0] = JumpAddress { .destination = destination, .is_offset = true };
    return result;
}


/// Decodes instructions that are part of the group 1 or group 2 sets, and that can operate on either a
/// register or a memory location.
Instruction group_r_m( unsigned char const *& instruction ) {
    struct HeaderBytes {
        // First byte
        unsigned char w : 1;
        unsigned char opcode : 7;
        // Second byte
        unsigned char r_m : 3;
        unsigned char subop : 3;
        unsigned char mod : 2;
    };
    auto const header { reinterpret_cast<HeaderBytes const *>( instruction ) };

    bool constexpr jumps[8] { false, false, true, true, true, true, false, false };
    bool const simple_mod { header->mod == 0b11 };
    bool const call_or_jmp { header->opcode == 0b111'1111 and jumps[header->subop] };

    Instruction result { instruction };
    instruction += 2;

    result.operands[0] = get_r_m( header->r_m, header->mod, header->w, instruction );
    if ( header->opcode == 0b111'1011 and header->subop == 0b000 ) // Test instruction
        result.operands[1] = get_immediate( instruction, header->w, true );

    result.write_far = call_or_jmp and (header->subop & 1);
    result.write_size = not call_or_jmp and not simple_mod;

    return result;
}


/// Decodes push and pop instructions that operate on segment registers.
Instruction push_pop_seg_reg( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char op1 : 3;
        unsigned char seg_reg : 2;
        unsigned char op2 : 3;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    Instruction result { instruction };
    ++instruction;
    result.operands[0] = SegmentRegister { header->seg_reg };
    return result;
}


/// Decodes in and out instructions.
Instruction in_out( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char w : 1;
        unsigned char d : 1;
        unsigned char one : 1;
        unsigned char variable : 1;
        unsigned char opcode : 4;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    Instruction result { instruction };
    result.name = get_mnemonic( instruction++ );

    result.operands[header->d] = Register { RegAX, bool( header->w ) };
    if ( header->variable )
        result.operands[1 - header->d] = Register { RegDX, true };
    else
        result.operands[1 - header->d] = get_immediate( instruction, false, false );

    return result;
}


/// Decodes instructions that are part of the shift set.
Instruction shift( unsigned char const *& instruction ) {
    struct HeaderBytes {
        // First byte
        unsigned char w : 1;
        unsigned char v : 1;
        unsigned char opcode : 6;
        // Second byte
        unsigned char r_m : 3;
        unsigned char subop : 3;
        unsigned char mod : 2;
    };
    auto const header { reinterpret_cast<HeaderBytes const *>( instruction ) };

    Instruction result { instruction };
    instruction += 2;
    result.write_size = header->mod != 0b11;

    result.operands[0] = get_r_m( header->r_m, header->mod, header->w, instruction );
    if ( header->v )
        result.operands[1] = Register { RegCL, 0 };
    else
        result.operands[1] = Immediate { 1, false };

    return result;
}


/// Decodes rep instructions.
Instruction repeat( unsigned char const *& instruction ) {
    struct SecondByte {
        unsigned char w : 1;
        unsigned char opcode : 7;
    };
    auto const values { reinterpret_cast<SecondByte const *>( instruction + 1 ) };

    std::string const next_mnemonic { get_mnemonic( instruction + 1 ) };
    char const width_modifier { values->w ? 'w' : 'b' };

    Instruction result { instruction, std::format( "rep {}{}", next_mnemonic, width_modifier ) };
    instruction += 2;
    return result;
}


/// Decodes esc instructions. Apparently nasm doesn't recognise these instructions, so this function has not
/// been tested at all.
Instruction escape( unsigned char const *& instruction ) {
    struct HeaderBytes {
        // First byte
        unsigned char op1 : 3;
        unsigned char opcode : 5;
        // Second byte
        unsigned char r_m : 3;
        unsigned char op2 : 3;
        unsigned char mod : 2;
    };
    auto const header { reinterpret_cast<HeaderBytes const *>( instruction ) };

    Instruction result { instruction };
    instruction += 2;

    if ( header->op2 != 0b111 )
        result.operands[0] = Immediate { header->op1 << 3 | header->op2 };
    result.operands[1] = get_r_m( header->r_m, header->mod, true, instruction );

    return result;
}


/// Decodes jmp and call instructions that jump a constant distance within the same segment.
Instruction direct_intrasegment( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char one : 1;
        unsigned char unwide : 1;
        unsigned char opcode: 6;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    Instruction result { instruction };
    ++instruction;

    int const destination { get_value( instruction, not header->unwide, false ) };
    result.operands[0] = JumpAddress { .destination = destination, .is_offset = true };
    return result;
}


/// Decodes jmp and call instructions that jump a constant distance to another segment.
Instruction direct_intersegment( unsigned char const *& instruction ) {
    Instruction result { instruction };
    ++instruction;
    int const ip { get_value( instruction, true, true ) };
    int const cs { get_value( instruction, true, true ) };
    result.operands[0] = JumpIntersegment { cs, ip };
    return result;
}


/// Forward declaration for prefix() to work with
Instruction decode( unsigned char const *& instruction );

/// Decodes a lock instruction, which is written as a prefix to the next instruction.
Instruction prefix( unsigned char const *& instruction ) {
    unsigned char const * const header { instruction };
    Instruction result { decode( ++instruction ) };
    result.bytes = header;
    result.name = get_mnemonic( header ) + ' ' + result.name;
    return result;
}


/// Decodes a segment instruction, which overrides the segment used in the next instruction.
Instruction segment_override( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char op1 : 3;
        unsigned char seg : 2;
        unsigned char op2 : 3;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    Instruction result { decode( ++instruction ) };

    bool success { false };
    for ( int index { 0 } ; index < 2; ++index ) {
        if ( std::holds_alternative<EffectiveAddress>( result.operands[index] ) ) {
            auto & address { std::get<AddressOperand>( result.operands[index] ) };
            address.has_override = true;
            address.seg_reg = header->seg;
            success = true;
        }
    }
    if ( not success )
        throw std::invalid_argument( "Segment override precedes un-segment-overridable instruction." );
    return result;
}


using DecoderFunction = std::function<Instruction(unsigned char const *&)>;

/// Creates a decoding table with functions/functors to handle every supported byte value.
std::array<DecoderFunction, 256> constexpr decoding_table() {
    std::array<DecoderFunction, 256> table { nullptr };

    for ( unsigned char j { 0b0000 }; j < 0b1000; ++j ) {
        unsigned char const subop ( j << 3 );
        // 00 01 02 03 08 09 0a 0b 10 11 12 13 18 19 1a 1b 20 21 22 23 28 29 2a 2b 30 31 32 33 38 39 3a 3b
        for ( unsigned char i { 0b000 }; i < 0b100; ++i )
            table[subop | i] = RmToRegDecoder {};
        // 04 05 0c 0d 14 15 1c 1d 24 25 2c 2d 34 35 3c 3d
        table[subop | 0b100] = ImmToAccumDecoder {};
        table[subop | 0b101] = ImmToAccumDecoder {};
    }
    // 06 07 0e 0f 16 17 1e 1f
    for ( unsigned char i { 0b000 }; i < 0b100; ++i ) {
        table[0b0000'0110 | (i << 3)] = push_pop_seg_reg;
        table[0b0000'0111 | (i << 3)] = push_pop_seg_reg;
    }
    // 27 2f 37 3f
    for ( unsigned char i { 0b000 }; i < 0b100; ++i )
        table[0b0010'0111 | (i << 3)] = MnemonicDecoder {};
    // 26 2e 36 3e
    for ( unsigned char i { 0b000 }; i < 0b100; ++i )
        table[0b0010'0110 | (i << 3)] = segment_override;
    // 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f 50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f
    for ( unsigned char i { 0x40 }; i < 0x60; ++i )
        table[i] = SingleRegDecoder {};
    // 70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f
    for ( unsigned char i { 0x70 }; i < 0x80; ++i )
        table[i] = jump_conditional;
    // 80 81 82 83
    for ( unsigned char i { 0x80 }; i < 0x84; ++i )
        table[i] = ImmToRmDecoder {};
    // 84 85 86 87 88 89 8a 8b
    for ( unsigned char i { 0x84 }; i < 0x8c; ++i )
        table[i] = RmToRegDecoder {};
    table[0x8c] = move_r_m_seg;
    table[0x8d] = RmToRegDecoder { .force_swap = true };
    table[0x8e] = move_r_m_seg;
    table[0x8f] = group_r_m;
    // 90 91 92 93 94 95 96 97
    for ( unsigned char i { 0x90 }; i < 0x98; ++i )
        table[i] = SingleRegDecoder { .has_operand = true, .operand = Register { RegAX, 1 } };
    table[0x98] = MnemonicDecoder {};
    table[0x99] = MnemonicDecoder {};
    table[0x9a] = direct_intersegment;
    // 9b 9c 9d 9e 9f
    for ( unsigned char i { 0x9b }; i < 0xa0; ++i )
        table[i] = MnemonicDecoder {};
    // a0 a1 a2 a3
    for ( unsigned char i { 0xa0 }; i < 0xa4; ++i )
        table[i] = ImmToAccumDecoder { .address = true };
    // a4 a5 a6 a7
    for ( unsigned char i { 0xa4 }; i < 0xa8; ++i )
        table[i] = MnemonicDecoder {};
    table[0xa8] = ImmToAccumDecoder {};
    table[0xa9] = ImmToAccumDecoder {};
    // aa ab ac ad ae af
    for ( unsigned char i { 0xaa }; i < 0b0; ++i )
        table[i] = MnemonicDecoder {};
    // b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf
    for ( unsigned char i { 0xb0 }; i < 0xc0; ++i )
        table[i] = move_immediate_reg;
    table[0xc2] = MnemonicDecoder { .has_value = true };
    table[0xc3] = MnemonicDecoder {};
    table[0xc4] = RmToRegDecoder { .force_swap = true, .force_wide = true };
    table[0xc5] = RmToRegDecoder { .force_swap = true, .force_wide = true };
    table[0xc6] = ImmToRmDecoder { .is_move = true };
    table[0xc7] = ImmToRmDecoder { .is_move = true };
    table[0xca] = MnemonicDecoder { .has_value = true };
    table[0xcb] = MnemonicDecoder {};
    table[0xcc] = MnemonicDecoder {};
    table[0xcd] = MnemonicDecoder { .has_value = true, .wide = false };
    table[0xce] = MnemonicDecoder {};
    table[0xcf] = MnemonicDecoder {};
    // d0 d1 d2 d3
    for ( unsigned char i { 0xd0 }; i < 0xd4; ++i )
        table[i] = shift;
    table[0xd4] = MnemonicDecoder { .header_bytes = 2 };
    table[0xd5] = MnemonicDecoder { .header_bytes = 2 };
    table[0xd7] = MnemonicDecoder {};
    // d8 d9 da db dc dd de
    for ( unsigned char i { 0xd8 }; i < 0xe0; ++i )
        table[i] = escape;
    // e0 e1 e2 e3
    for ( unsigned char i { 0xe0 }; i < 0xe4; ++i )
        table[i] = jump_conditional;
    // e4 e5 e6 e7 ec ed ee ef
    for ( unsigned char i { 0b000 }; i < 0b100; ++i ) {
        table[0xe4 | i] = in_out;
        table[0xec | i] = in_out;
    }
    table[0xe8] = direct_intrasegment;
    table[0xe9] = direct_intrasegment;
    table[0xea] = direct_intersegment;
    table[0xeb] = direct_intrasegment;
    table[0xf0] = prefix;
    table[0xf2] = repeat;
    table[0xf3] = repeat;
    table[0xf4] = MnemonicDecoder {};
    table[0xf5] = MnemonicDecoder {};
    table[0xf6] = group_r_m;
    table[0xf7] = group_r_m;
    // f8 f9 fa fb fc fd
    for ( unsigned char i { 0xf8 }; i < 0xfe; ++i )
        table[i] = MnemonicDecoder {};
    table[0xfe] = group_r_m;
    table[0xff] = group_r_m;

#if ( 0 )
    std::println( "; Current table status:" );
    std::println( "; +----------------+" );
    for ( unsigned int row { 0 }; row < 16; ++row ) {
        std::print( "; |" );
        for ( unsigned int col { 0 }; col < 16; ++col ) {
            unsigned int const index { row << 4 | col };
            std::print( "{}", (table[index] ? '#' : ' ') );
        }
        std::println( "|" );
    }
    std::println( "; +----------------+" );
    std::println();
#endif

    return table;
};


/// Decodes a single instruction (or 2 if the first one is a prefix instruction).
Instruction decode( unsigned char const *& instruction ) {
    static auto const table { decoding_table() };

    auto const & decoder { table[*instruction] };
    if ( decoder )
        return decoder( instruction );

    throw std::invalid_argument( std::format( "Unsupported instruction {:#04x}.", *instruction ) );
}


/// Decodes all instructions in the given array, and prints them to the console in assembly language. The
/// original bytes are also written next to their respective assembly lines as comments. This function returns
/// a Boolean indicating whether the decoding was a success.
std::vector<Instruction> decode_all( unsigned char const * const instructions,
                                     unsigned char const * const end,
                                     bool print_instructions ) {
    std::vector<Instruction> assembly {};
    std::vector<std::string> bytes {};
    std::set<unsigned int> labels {};

    unsigned char const * instruction { instructions };
    unsigned char const * previous { instructions };

    while ( instruction != end ) {
        Instruction result { instruction };
        try {
            result = decode( instruction );
        } catch ( std::invalid_argument const & exception ) {
            std::println( "An exception was thrown: {}", exception.what() );
            break;
        }

        if ( instruction == previous ) {
            std::println( "The decoder failed to read any data, aborting." );
            break;
        }

        // Test for jumps, and record the destination
        if ( std::holds_alternative<JumpAddress>( result.operands[0] ) ) {
            int const current ( instruction - instructions );

            // Only replace the offset with a label if the destination is within the program.
            // I have no idea whether this is generally true for actual programs, but the
            // challenges do contain some offsets that exceed the size of the program.
            // For these large offsets, the actual destination can still be computed however.
            auto & destination { std::get<JumpAddress>( result.operands[0] ) };
            if ( destination.is_offset ) {
                destination.destination += current;
                destination.is_offset = false;
            }
            if ( destination.destination <= end - instructions ) {
                labels.emplace( destination.destination );
                destination.in_bounds = true;
            }
        }

        // Collect the generated assembly code
        assembly.push_back( result );

        // Keep track of the consumed bytes
        bytes.emplace_back();
        while ( previous != instruction )
            bytes.back() += std::format( " {:#04x}", *(previous++) );
    }

    if ( not print_instructions )
        return assembly;

    auto label_iter { labels.cbegin() };
    unsigned int byte_counter { 0 };

    // Actually print the generated assembly code, with the respective byte code next to it
    for ( unsigned int line { 0 }; line < assembly.size(); ++line ) {
        // Insert labels where necessary, with names generated from their location in number of bytes
        if ( label_iter != labels.cend() and *label_iter == byte_counter ) {
            std::println( "label_{}:", byte_counter );
            ++label_iter;
        }

#if ( 1 ) // Toggle whether the machine code is written as comments next to the generated assembly
        std::println( "{:40};{:40}< Byte {}", to_string( assembly[line] ), bytes[line], byte_counter );
#else
        std::println( "{}", to_string( assembly[line] ) );
#endif
        // All machine code comments are formatted as " 0x__", i.e. length 5
        byte_counter += bytes[line].size() / 5;
    }
    return assembly;
}

