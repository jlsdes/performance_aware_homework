#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <print>
#include <set>
#include <string>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Big tables
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <std::size_t table_size>
using StringTable = std::array<std::string, table_size>;

// Page 178 of https://edge.edx.org/c4x/BITSPilani/EEE231/asset/8086_family_Users_Manual_1_.pdf
StringTable<256> constexpr mnemonics {
    "add", "add", "add", "add", "add", "add", "push", "pop", "or", "or", "or", "or", "or", "or", "push", "ERR",
    "adc", "adc", "adc", "adc", "adc", "adc", "push", "pop", "sbb", "sbb", "sbb", "sbb", "sbb", "sbb", "push", "pop",
    "and", "and", "and", "and", "and", "and", "seg", "daa", "sub", "sub", "sub", "sub", "sub", "sub", "seg", "das",
    "xor", "xor", "xor", "xor", "xor", "xor", "seg", "aaa", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "seg", "aas",
    "inc", "inc", "inc", "inc", "inc", "inc", "inc", "inc", "dec", "dec", "dec", "dec", "dec", "dec", "dec", "dec",
    "push", "push", "push", "push", "push", "push", "push", "push", "pop", "pop", "pop", "pop", "pop", "pop", "pop", "pop",
    "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR",
    "jo", "jno", "jb", "jnb", "je", "jne", "jbe", "jnbe", "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jnle",
    "IMM", "IMM", "IMM", "IMM", "test", "test", "xchg", "xchg", "mov", "mov", "mov", "mov", "mov", "lea", "mov", "pop",
    "xchg", "xchg", "xchg", "xchg", "xchg", "xchg", "xchg", "xchg", "cbw", "cwd", "call", "wait", "pushf", "popf", "sahf", "lahf",
    "mov", "mov", "mov", "mov", "movs", "movs", "cmps", "cmps", "test", "test", "stos", "stos", "lods", "lods", "scas", "scas",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "ERR", "ERR", "ret", "ret", "les", "lds", "mov", "mov", "ERR", "ERR", "ret", "ret", "int", "int", "into", "iret",
    "SHIFT", "SHIFT", "SHIFT", "SHIFT", "aam", "aad", "ERR", "xlat", "esc", "esc", "esc", "esc", "esc", "esc", "esc", "esc",
    "loopnz", "loopz", "loop", "jcxz", "in", "in", "out", "out", "call", "jmp", "jmp", "jmp", "in", "in", "out", "out",
    "lock", "ERR", "rep", "rep", "hlt", "cmc", "GRP1", "GRP1", "clc", "stc", "cli", "sti", "cld", "std", "GRP2", "GRP2",
};

StringTable<8> constexpr immediate_table {
    "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp",
};

StringTable<8> constexpr shift_table {
    "rol", "ror", "rcl", "rcr", "shl", "shr", "ERR", "sar",
};

StringTable<8> constexpr group_1_table {
    "test", "ERR", "not", "neg", "mul", "imul", "div", "idiv",
};

StringTable<8> constexpr group_2_table {
    "inc", "dec", "call", "call", "jmp", "jmp", "push", "ERR",
};

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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Utility functions and arrays
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Register field encoding, where indices are formed by 4 bits: <w><reg>
StringTable<16> constexpr reg_names {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
};


// Base effective address calculations
StringTable<8> constexpr reg_sums {
    "bx + si",
    "bx + di",
    "bp + si",
    "bp + di",
    "si",
    "di",
    "bp",
    "bx",
};


// Segment register names
StringTable<4> constexpr seg_reg_names {
    "es",
    "cs",
    "ss",
    "ds",
};


inline int get_value( unsigned char const *& instruction, bool const w, bool const s ) {
    int result;
    switch ( w << 1 | s ) {
    case 0b00:
        return *(instruction++);
    case 0b01:
        return *reinterpret_cast<char const *>( instruction++ );
    case 0b10:
        result = *reinterpret_cast<unsigned short const *>( instruction );
        break;
    case 0b11:
        result = *reinterpret_cast<short const *>( instruction );
        break;
    }
    instruction += 2;
    return result;
}


std::string get_r_m_name( unsigned char const r_m,
                          unsigned char const mod,
                          unsigned char const w,
                          unsigned char const *& instruction ) {
    if ( mod == 0b11 )
        return reg_names[(w << 3) | r_m];

    bool const direct_address { mod == 0b00 and r_m == 0b110 };

    bool const no_displacement { mod == 0b00 and not direct_address };
    bool const wide_displacement { mod == 0b10 or direct_address };
    int const displacement { no_displacement ? 0 : get_value( instruction, wide_displacement, true ) };

    std::stringstream stream {};
    stream << '[';
    if ( direct_address )
        stream << displacement;
    else {
        stream << reg_sums[r_m];
        if ( displacement != 0 )
            stream << (displacement < 0 ? " - " : " + ") << std::abs( displacement );
    }
    stream << ']';
    return stream.str();
};


std::string _decode_r_m_to_reg( unsigned char const *& instruction, bool const force_swap = false ) {
    struct Instruction {
        // First byte
        unsigned char w : 1;
        unsigned char d : 1;
        unsigned char opcode : 6;
        // Second byte
        unsigned char r_m : 3;
        unsigned char reg : 3;
        unsigned char mod : 2;
    };
    auto const values { reinterpret_cast<Instruction const *>( instruction ) };

    std::string const mnemonic { get_mnemonic( instruction ) };
    instruction += sizeof( Instruction );
    std::string source { reg_names[(values->w << 3) | values->reg] };
    std::string destination { get_r_m_name( values->r_m, values->mod, values->w, instruction ) };
    if ( force_swap or values->d )
        std::swap( source, destination );

    return std::format( "{} {}, {}", mnemonic, destination, source );
}

std::string decode_r_m_to_reg( unsigned char const *& instruction ) {
    return _decode_r_m_to_reg( instruction, false );
}

std::string decode_r_m_to_reg_swap( unsigned char const *& instruction ) {
    return _decode_r_m_to_reg( instruction, true );
}


std::string decode_imm_to_r_m( unsigned char const *& instruction ) {
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
    instruction += sizeof( Instruction );

    std::string const destination { get_r_m_name( values->r_m, values->mod, values->w, instruction ) };
    std::string const source_type { values->w ? "word" : "byte" };

    if ( values->opcode == 0b110001 ) {
        int const source { get_value( instruction, values->w, values->s ) };
        return std::format( "{}, {} {}", destination, source_type, source );
    } else {
        int const source { get_value( instruction, values->s == 0 and values->w == 1, values->s ) };
        if ( values->mod == 0b11 )
            return std::format( "{}, {}", destination, source );
        else
            return std::format( "{} {}, {}", source_type, destination, source );
    }
}


std::string decode_imm_to_accum( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char w : 1;
        unsigned char d : 1;
        unsigned char opcode : 6;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };
    instruction += sizeof( HeaderByte );

    std::string source { std::to_string( get_value( instruction, header->w, true ) ) };
    if ( header->opcode == 0b101000 )
        source = '[' + source + ']';
    std::string destination { header->w ? "ax" : "al" };
    if ( header->d )
        std::swap( source, destination );

    return std::format( "{}, {}", destination, source );
}


std::string decode_only_mnemonic( unsigned char const *& instruction ) {
    return get_mnemonic( instruction++ );
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Move instructions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string move_immediate_reg( unsigned char const *& instruction ) {
    struct Instruction {
        // Only byte
        unsigned char reg : 3;
        unsigned char w : 1;
        unsigned char opcode : 4;
    };
    auto const values { reinterpret_cast<Instruction const *>( instruction ) };
    ++instruction;

    if ( values->opcode != 0b1011 )
        return "";

    unsigned char const destination ( (values->w << 3) | values->reg );
    int const source { get_value( instruction, values->w, true ) };

    return std::format( "mov {}, {}", reg_names[destination], source );
}


std::string move_immediate_r_m( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char w : 1;
        unsigned char opcode : 7;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    if ( header->opcode != 0b1100011 )
        return "";

    return std::format( "mov {}", decode_imm_to_r_m( instruction ) );
}


std::string move_accumulator( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char w : 1;
        unsigned char d : 1;
        unsigned char opcode : 6;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    if ( header->opcode != 0b101000 )
        return "";

    return std::format( "mov {}", decode_imm_to_accum( instruction ));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Arithmetic instructions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string arithmetic_immediate_r_m( unsigned char const *& instruction ) {
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

    if ( values->opcode != 0b100000 )
        return "";

    return std::format( "{} {}", get_mnemonic( instruction ), decode_imm_to_r_m( instruction ) );
}


std::string arithmetic_accumulator( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char w : 1;
        unsigned char op2 : 2;
        unsigned char subop : 3;
        unsigned char op1 : 2;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    if ( header->op1 != 0b00 and header->op2 != 0b10 )
        return "";

    return std::format( "{} {}", get_mnemonic( instruction ), decode_imm_to_accum( instruction ) );
};
 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Jump instructions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string jump_conditional( unsigned char const *& instruction ) {
    unsigned char constexpr index_masks[2] { 0b0000'1111, 0b0000'0011 };
    unsigned char constexpr opcode_masks[2] { 0b1111'0000, 0b1111'1100 };
    unsigned char constexpr opcodes[2] { 0b0111'0000, 0b1110'0000 };

    bool const first_bit ( *instruction & 0b1000'0000 );
    if ( (*instruction & opcode_masks[first_bit]) != opcodes[first_bit] )
        return "";

    std::string const mnemonic { get_mnemonic( instruction ) };
    int const offset { get_value( ++instruction, false, true ) };
    return std::format( "{} ${:+}", mnemonic, offset + 2 );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Group 2 operations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string group_2_r_m( unsigned char const *& instruction ) {
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
    std::string const value_type { values->mod == 0b11 ? "" : values->w ? "word " : "byte " };

    return std::format( "{} {}{}", mnemonic, value_type, r_m );
}


std::string group_2_reg( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char reg : 3;
        unsigned char opcode : 5;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    std::string const mnemonic { get_mnemonic( instruction ) };
    ++instruction;
    return std::format( "{} {}", mnemonic, reg_names[0b0000'1000 | header->reg] );
}


std::string push_pop_seg_reg( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char op1 : 3;
        unsigned char seg_reg : 2;
        unsigned char op2 : 3;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    std::string const mnemonic { get_mnemonic( instruction ) };
    ++instruction;
    return std::format( "{} {}", mnemonic, seg_reg_names[header->seg_reg] );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Exchange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string exchange_reg_imm( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char reg : 3;
        unsigned char opcode : 5;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    if ( header->opcode != 0b1'0010 )
        return "";

    ++instruction;
    return std::format( "xchg ax, {}", reg_names[0b0000'1000 | header->reg] );
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Input / output
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string in_out( unsigned char const *& instruction ) {
    struct HeaderByte {
        unsigned char w : 1;
        unsigned char d : 1;
        unsigned char one : 1;
        unsigned char variable : 1;
        unsigned char opcode : 4;
    };
    auto const header { reinterpret_cast<HeaderByte const *>( instruction ) };

    if ( header->opcode != 0b1110 or header->one != 1 )
        return "";

    std::string const mnemonic { get_mnemonic( instruction ) };
    instruction += sizeof( HeaderByte );
    std::string destination { header->w ? "ax" : "al" };
    std::string source { header->variable ? "dx" : std::to_string( get_value( instruction, false, false ) ) };
    if ( header->d )
        std::swap( destination, source );

    return std::format( "{} {}, {}", mnemonic, destination, source );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// General decoding
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::array<std::function<std::string(unsigned char const *&)>, 256> constexpr decoding_table() {
    std::array<std::function<std::string(unsigned char const *&)>, 256> table { nullptr };

    // 00___0__
    // 00___10_
    for ( unsigned char j { 0 }; j < (1 << 3); ++j ) {
        unsigned char const subop ( j << 3 );
        for ( unsigned char i { 0 }; i < (1 << 2); ++i )
            table[0b0000'0000 | i | subop] = decode_r_m_to_reg;
        for ( unsigned char i { 0 }; i < (1 << 1); ++i )
            table[0b0000'0100 | i | subop] = arithmetic_accumulator;
    }
    // 000__11_
    for ( unsigned char i { 0 }; i < (1 << 3); ++i )
        table[0b0000'0110 | i | (i << 2)] = push_pop_seg_reg;
    // 001_0111
    for ( unsigned char i { 0 }; i < (1 << 1); ++i )
        table[0b0010'0111 | (i << 4)] = decode_only_mnemonic;
    // 01000___
    for ( unsigned char i { 0 }; i < (1 << 3); ++i )
        table[0b0100'0000 | i] = group_2_reg;
    // 0101____
    for ( unsigned char i { 0 }; i < (1 << 4); ++i )
        table[0b0101'0000 | i] = group_2_reg;
    // 0111____
    for ( unsigned char i { 0 }; i < (1 << 4); ++i )
        table[0b0111'0000 | i] = jump_conditional;
    // 100000__
    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        table[0b1000'0000 | i] = arithmetic_immediate_r_m;
    // 1000011_
    for ( unsigned char i { 0 }; i < (1 << 1); ++i )
        table[0b1000'0110 | i] = decode_r_m_to_reg;
    // 100010__
    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        table[0b1000'1000 | i] = decode_r_m_to_reg;
    // 10001101
    table[0b1000'1101] = decode_r_m_to_reg_swap;
    // 10001111
    table[0b1000'1111] = group_2_r_m;
    // 10010___
    for ( unsigned char i { 0 }; i < (1 << 3); ++i )
        table[0b1001'0000 | i] = exchange_reg_imm;
    // 100111__
    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        table[0b1001'1100 | i] = decode_only_mnemonic;
    // 101000__
    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        table[0b1010'0000 | i] = move_accumulator;
    // 1011____
    for ( unsigned char i { 0 }; i < (1 << 4); ++i )
        table[0b1011'0000 | i] = move_immediate_reg;
    // 1100010_
    for ( unsigned char i { 0 }; i < (1 << 1); ++i )
        table[0b1100'0100 | i] = decode_r_m_to_reg_swap;
    // 1100011_
    for ( unsigned char i { 0 }; i < (1 << 1); ++i )
        table[0b1100'0110 | i] = move_immediate_r_m;
    // 11010111
    table[0b1101'0111] = decode_only_mnemonic;
    // 111000__
    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        table[0b1110'0000 | i] = jump_conditional;
    // 1110_1__
    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        for ( unsigned char j { 0 }; j < (1 << 1); ++j )
            table[0b1110'0100 | i | (j << 3)] = in_out;
    // 1111111_
    for ( unsigned char i { 0 }; i < (1 << 1); ++i )
        table[0b1111'1110 | i] = group_2_r_m;

#if ( true )
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


std::string decode( unsigned char const *& instruction ) {
    static auto const table { decoding_table() };

    auto const & decoder { table[*instruction] };
    if ( decoder )
        return decoder( instruction );

    return std::format( "Unsupported instruction {:#04x}.", *instruction );
}


bool decode_all( unsigned char const * const instructions, unsigned char const * const end ) {
    std::vector<std::string> assembly {};
    std::vector<std::string> bytes {};
    std::set<unsigned int> labels {};

    unsigned char const * instruction { instructions };
    unsigned char const * previous { instructions };

    bool success { true };
    while ( instruction != end ) {
        std::string result { decode( instruction ) };
        success = not result.empty() and instruction != previous;

        // Test for jumps, and record the destination
        if ( result.contains( '$' ) ) {
            auto const loc { result.find_first_of( '$' ) };
            int const offset { std::stoi( result.substr( loc + 1 ) ) };
            int const label_id ( (previous - instructions) + offset );
            labels.emplace( label_id );
            result = std::format( "{}label_{}", result.substr( 0, loc ), label_id );
        }

        // Collect the generated assembly code
        assembly.push_back( result );

        // Keep track of the consumed bytes
        bytes.emplace_back();
        while ( previous != instruction )
            bytes.back() += std::format( " {:#04x}", *(previous++) );

        if ( not success )
            break;
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
        byte_counter += bytes[line].size() / 5; // All machine code comments are formatted as " 0x__"
        std::println( "{:40};{}", assembly[line], bytes[line] );
    }
    return success;
}


int main( int const argc, char const * const * const argv ) {
    if ( argc != 2 ) {
        std::println( "Invalid number of arguments, expected exactly 1." );
        return 1;
    }

    std::filesystem::path const path { argv[1] };
    if ( not std::filesystem::exists( path ) ) {
        std::println( "File {} does not exist.", argv[1] );
        return 1;
    }

    std::ifstream file { path, std::ios_base::binary };
    if ( not file.is_open() ) {
        std::println( "Failed to open file {}.", argv[1] );
        return 1;
    }

    std::size_t const size { std::filesystem::file_size( path ) };
    unsigned char * const instructions { new unsigned char[size] {} };
    file.read( reinterpret_cast<char *>(instructions), size );

    std::println( "; Disassembly for {}", argv[1] );
    std::println( "bits 16");
    std::println();

    decode_all( instructions, instructions + size );

    delete[] instructions;
    return 0;
}
