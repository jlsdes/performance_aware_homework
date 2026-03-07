#include "decoder.hpp"
#include "instruction.hpp"

#include <format>
#include <functional>
#include <print>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>


/// Returns the mnemonic for the instruction.
std::string constexpr get_mnemonic( unsigned char const * const instruction ) {
    switch ( *instruction ) {
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
        return immediate_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    case 0xd0:
    case 0xd1:
    case 0xd2:
    case 0xd3:
        return shift_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    case 0xf6:
    case 0xf7:
        return group_1_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    case 0xfe:
    case 0xff:
        return group_2_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    default:
        return mnemonics[*instruction];
    }
}


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

        Instruction result { get_mnemonic( instruction ) };
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
    bool mid_type { false };

    Instruction operator()( unsigned char const *& instruction ) {
        struct Instruction {
            // First byte
            unsigned char w : 1;
            unsigned char s : 1;
            unsigned char opcode : 6;
            // Second byte
            unsigned char r_m : 3;
            unsigned char subop : 3;
            unsigned char mod : 2;
        };
        auto const values { reinterpret_cast<Instruction const *>( instruction ) };

        std::string const mnemonic { get_mnemonic( instruction ) };
        instruction += sizeof( Instruction );
        std::string const destination { get_r_m_name( values->r_m, values->mod, values->w, instruction ) };
        std::string const source_type { values->w ? "word" : "byte" };

        if ( mid_type ) {
            int const source { get_value( instruction, values->w, values->s ) };
            return { std::format( "{} {}, {} {}", mnemonic, destination, source_type, source ) };
        }
        int const source { get_value( instruction, values->s == 0 and values->w == 1, values->s ) };
        if ( values->mod == 0b11 )
            return { std::format( "{} {}, {}", mnemonic, destination, source ) };
        else
            return { std::format( "{} {} {}, {}", mnemonic, source_type, destination, source ) };
    }
};


/// Decodes instructions that have the structure defined in the 'Instruction' struct below. These operate on
/// an accumulator register and a register or memory location.
/// The 'bracketed' attribute indicates whether the following byte(s) contain a memory location or an actual
/// immediate value.
struct ImmToAccumDecoder {
    bool bracketed { false };

    Instruction operator()( unsigned char const *& instruction ) {
        struct HeaderByte {
            unsigned char w : 1;
            unsigned char d : 1;
            unsigned char opcode : 6;
        };
        auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

        std::string const mnemonic { get_mnemonic( instruction ) };
        instruction += sizeof( HeaderByte );
        std::string source { std::to_string( get_value( instruction, header->w, true ) ) };
        if ( bracketed )
            source = '[' + source + ']';
        std::string destination { header->w ? "ax" : "al" };
        if ( header->d )
            std::swap( source, destination );

        return { std::format( "{} {}, {}", mnemonic, destination, source ) };
    }
};


/// Decodes instructions that have the structure defined in the 'Instruction' struct below. These operate on
/// single byte instructions that have the register ID within that first byte.
/// The 'operand' attribute can be used to add a static operand between the mnemonic and the register name.
struct SingleRegDecoder {
    std::string operand { "" };

    Instruction operator()( unsigned char const *& instruction ) {
        struct HeaderByte {
            unsigned char reg : 3;
            unsigned char opcode : 5;
        };
        auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

        std::string const mnemonic { get_mnemonic( instruction ) };
        ++instruction;
        return { std::format( "{} {}{}", mnemonic, operand, reg_names[0b1000 | header->reg] ) };
    };
};


/// Decodes simple instructions that either have no operand, or just one value in the following byte(s).
struct MnemonicDecoder {
    unsigned int header_bytes { 1 };
    bool has_value { false };
    bool wide { true };
    bool sign { true };

    Instruction operator()( unsigned char const *& instruction ) {
        std::string const mnemonic { get_mnemonic( instruction ) };
        instruction += header_bytes;
        if ( has_value )
            return { std::format( "{} {}", mnemonic, get_value( instruction, wide, sign ) ) };
        return { mnemonic };
    };
};


/// Decodes move instructions that simply move an immediate value into a register.
Instruction move_immediate_reg( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char reg : 3;
        unsigned char w : 1;
        unsigned char opcode : 4;
    };
    auto const values { reinterpret_cast<HeaderByte const *>( instruction ) };
    ++instruction;

    unsigned char const destination ( (values->w << 3) | values->reg );
    int const source { get_value( instruction, values->w, true ) };

    return { std::format( "mov {}, {}", reg_names[destination], source ) };
}


/// Decodes move instructions that use a segment register.
Instruction move_r_m_seg( unsigned char const *& instruction ) {
    struct Instruction {
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
    auto const values { reinterpret_cast<Instruction const *>( instruction ) };

    instruction += sizeof( Instruction );
    std::string const destination { get_r_m_name( values->r_m, values->mod, true, instruction ) };
    std::string const source { seg_reg_names[values->seg_reg] };

    return { std::format( "mov {}, {}", destination, source ) };
}
 

/// Decodes conditional jump instructions. The offset values are relative to the end of the instruction in
/// machine code, but to the start of the instruction in assembly, so we still need to add 2 to get correct
/// offsets. (All instructions that are decoded by this function are exactly 2 bytes large.)
/// Also, there is a post-processing step where these offsets are converted into labels if possible.
Instruction jump_conditional( unsigned char const *& instruction ) {
    std::string const mnemonic { get_mnemonic( instruction ) };
    int const offset { get_value( ++instruction, false, true ) };

    return { std::format( "{} ${:+}", mnemonic, offset + 2 ) };
}


/// Decodes instructions that are part of the group 1 or group 2 sets, and that can operate on either a
/// register or a memory location.
Instruction group_r_m( unsigned char const *& instruction ) {
    struct Instruction {
        // First byte
        unsigned char w : 1;
        unsigned char opcode : 7;
        // Second byte
        unsigned char r_m : 3;
        unsigned char subop : 3;
        unsigned char mod : 2;
    };
    auto const values { reinterpret_cast<Instruction const *>( instruction ) };

    std::string const mnemonic { get_mnemonic( instruction ) };
    instruction += sizeof( Instruction );
    std::string const r_m { get_r_m_name( values->r_m, values->mod, values->w, instruction ) };

    bool const simple_mod { values->mod == 0b11 };
    bool const call_or_jmp { values->opcode == 0b111'1111 and (values->subop & 0b100) ^ ((values->subop & 0b010) << 1) };

    std::string value_type {};
    if ( call_or_jmp )
        value_type = values->subop & 1 ? "far " : "";
    else if ( not simple_mod )
        value_type = values->w ? "word " : "byte ";

    if ( values->opcode == 0b1111'011 and values->subop == 0b000 ) { // Special case: test instruction
        int const test_value { get_value( instruction, values->w, true ) };
        return { std::format( "{} {}{}, {}", mnemonic, value_type, r_m, test_value ) };
    }
    return { std::format( "{} {}{}", mnemonic, value_type, r_m ) };
}


/// Decodes push and pop instructions that operate on segment registers.
Instruction push_pop_seg_reg( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char op1 : 3;
        unsigned char seg_reg : 2;
        unsigned char op2 : 3;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    Instruction result { get_mnemonic( instruction++ ) };
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

    Instruction result {};
    result.name = get_mnemonic( instruction++ );

    result.operands[header->d] = Register { RegA, bool( header->w ) };
    if ( header->variable )
        result.operands[1 - header->d] = Register { RegD, true };
    else
        result.operands[1 - header->d] = get_value( instruction, false, false );

    return result;
}


/// Decodes instructions that are part of the shift set.
Instruction shift( unsigned char const *& instruction ) {
    struct Instruction {
        // First byte
        unsigned char w : 1;
        unsigned char v : 1;
        unsigned char opcode : 6;
        // Second byte
        unsigned char r_m : 3;
        unsigned char subop : 3;
        unsigned char mod : 2;
    };
    auto const values { reinterpret_cast<Instruction const *>( instruction ) };

    std::string const mnemonic { get_mnemonic( instruction ) };
    instruction += sizeof( Instruction );
    std::string const source { values->v ? "cl" : "1" };
    std::string const value_type { values->mod == 0b11 ? "" : values->w ? "word " : "byte " };
    std::string const destination { get_r_m_name( values->r_m, values->mod, values->w, instruction ) };

    return { std::format( "{} {}{}, {}", mnemonic, value_type, destination, source ) };
}


/// Decodes rep instructions.
Instruction repeat( unsigned char const *& instruction ) {
    struct SecondByte {
        unsigned char w : 1;
        unsigned char opcode : 7;
    };
    auto const values { reinterpret_cast<SecondByte const *>( instruction + 1 ) };

    std::string const mnemonic { get_mnemonic( instruction++ ) };
    std::string const name { get_mnemonic( instruction++ ) };

    return { std::format( "{} {}{}", mnemonic, name, values->w ? 'w' : 'b' ) };
}


/// Decodes esc instructions. Apparently nasm doesn't recognise these instructions, so this function has not
/// been tested at all.
Instruction escape( unsigned char const *& instruction ) {
    struct Instruction {
        // First byte
        unsigned char op1 : 3;
        unsigned char opcode : 5;
        // Second byte
        unsigned char r_m : 3;
        unsigned char op2 : 3;
        unsigned char mod : 2;
    };
    auto const values { reinterpret_cast<Instruction const *>( instruction ) };

    std::string const mnemonic { get_mnemonic( instruction ) };
    instruction += sizeof( Instruction );
    std::string const source { get_r_m_name( values->r_m, values->mod, true, instruction ) };

    if ( values->op2 == 0b111 )
        return { "esc" };
    return { std::format( "esc {}, {}", (values->op1 << 3 | values->op2), source ) };
}


/// Decodes jmp and call instructions that jump a constant distance within the same segment.
Instruction direct_intrasegment( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char one : 1;
        unsigned char unwide : 1;
        unsigned char opcode: 6;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    std::string const mnemonic { get_mnemonic( instruction ) };
    instruction += sizeof( HeaderByte );
    int const ip_inc { get_value( instruction, not header->unwide, false ) };

    return { std::format( "{} ${:+}", mnemonic, ip_inc + 3 ) };
}


/// Decodes jmp and call instructions that jump a constant distance to another segment.
Instruction direct_intersegment( unsigned char const *& instruction ) {
    std::string const mnemonic { get_mnemonic( instruction ) };
    ++instruction;
    int const ip { get_value( instruction, true, true ) };
    int const cs { get_value( instruction, true, true ) };

    return { std::format( "{} {}:{}", mnemonic, cs, ip ) };
}


/// Forward declaration for prefix() to work with
Instruction decode( unsigned char const *& instruction );

/// Decodes a lock instruction, which is written as a prefix to the next instruction.
Instruction prefix( unsigned char const *& instruction ) {
    std::string const mnemonic { get_mnemonic( instruction ) };
    ++instruction;
    std::stringstream stream {};
    stream << decode( instruction );
    return { std::format( "{} {}", mnemonic, stream.str() ) };
}


/// Decodes a segment instruction, which overrides the segment used in the next instruction.
Instruction segment_override( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char op1 : 3;
        unsigned char seg : 2;
        unsigned char op2 : 3;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    std::string result { (std::stringstream {} << decode( ++instruction) ).str() };
    std::string const segment { seg_reg_names[header->seg] + ':' };

    // Insert the segment register name right before the memory address, which starts with [
    auto const mem_location { std::find( result.cbegin(), result.cend(), '[') };
    result.insert( mem_location, segment.cbegin(), segment.cend() );
    return { result };
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
        table[i] = SingleRegDecoder { .operand = "ax, " };
    table[0x98] = MnemonicDecoder {};
    table[0x99] = MnemonicDecoder {};
    table[0x9a] = direct_intersegment;
    // 9b 9c 9d 9e 9f
    for ( unsigned char i { 0x9b }; i < 0xa0; ++i )
        table[i] = MnemonicDecoder {};
    // a0 a1 a2 a3
    for ( unsigned char i { 0xa0 }; i < 0xa4; ++i )
        table[i] = ImmToAccumDecoder { .bracketed = true };
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
    table[0xc6] = ImmToRmDecoder { .mid_type = true };
    table[0xc7] = ImmToRmDecoder { .mid_type = true };
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

#if ( 1 )
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

    return Instruction { std::format( "Unsupported instruction {:#04x}.", *instruction ) };
}


/// Decodes all instructions in the given array, and prints them to the console in assembly language. The
/// original bytes are also written next to their respective assembly lines as comments. This function returns
/// a Boolean indicating whether the decoding was a success.
bool decode_all( unsigned char const * const instructions, unsigned char const * const end ) {
    std::vector<std::string> assembly {};
    std::vector<std::string> bytes {};
    std::set<unsigned int> labels {};

    unsigned char const * instruction { instructions };
    unsigned char const * previous { instructions };

    bool success { true };
    while ( instruction != end ) {
        Instruction result { decode( instruction ) };
        std::string const result_str { to_string( result ) };

        if ( instruction == previous ) {
            assembly.push_back( "The decoder failed to read any data, aborting." );
            break;
        }

        // Test for jumps, and record the destination
        if ( result_str.contains( '$' ) ) {
            auto const loc { result_str.find_first_of( '$' ) };
            int const offset { std::stoi( result_str.substr( loc + 1 ) ) };
            int const label_id ( (previous - instructions) + offset );

            // Only replace the offset with a label if the destination is within the program.
            // I have no idea whether this is generally true for actual programs, but the
            // challenges do contain some offsets that exceed the size of the program.
            // For these large offsets, the actual destination can still be computed however.
            if ( label_id <= end - instructions ) {
                labels.emplace( label_id );
                result.name = std::format( "{}label_{}", result_str.substr( 0, loc ), label_id );
            } else {
                result.name = std::format( "{}{}", result_str.substr( 0, loc ), label_id );
            }
        }

        // Collect the generated assembly code
        assembly.push_back( result_str );

        // Keep track of the consumed bytes
        bytes.emplace_back();
        while ( previous != instruction )
            bytes.back() += std::format( " {:#04x}", *(previous++) );
    }

    auto label_iter { labels.cbegin() };
    unsigned int byte_counter { 0 };

    // Actually print the generated assembly code, with the respective byte code next to it
    for ( unsigned int line { 0 }; line < assembly.size(); ++line ) {
        // Insert labels where necessary, with names generated from their location in number of bytes
        if ( label_iter != labels.cend() and *label_iter == byte_counter ) {
            std::println( "label_{}:", byte_counter );
            ++label_iter;
        }
        // All machine code comments are formatted as " 0x__", i.e. length 5
        byte_counter += bytes[line].size() / 5;

#if ( 1 ) // Toggle whether the machine code is written as comments next to the generated assembly
        std::println( "{:40};{}", assembly[line], bytes[line] );
#else
        std::println( "{}", assembly[line] );
#endif
    }
    return success;
}

