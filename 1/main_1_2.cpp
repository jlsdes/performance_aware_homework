#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <print>
#include <sstream>
#include <string>


// Register field encoding, where indices are formed by 4 bits: <w><reg>
std::array<std::string, 16> constexpr reg_names {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
};

// Base effective address calculations
std::array<std::string, 8> constexpr reg_sums {
    "bx + si",
    "bx + di",
    "bp + si",
    "bp + di",
    "si",
    "di",
    "bp",
    "bx",
};


inline int signed_byte_value( unsigned char const *& instruction ) {
    return *reinterpret_cast<char const *>( instruction++ );
}

inline int unsigned_byte_value( unsigned char const *& instruction ) {
    return *(instruction++);
}

inline int signed_word_value( unsigned char const *& instruction ) {
    int const result { *reinterpret_cast<short int const *>( instruction ) };
    instruction += 2;
    return result;
}

inline int unsigned_word_value( unsigned char const *& instruction ) {
    int const result { *reinterpret_cast<unsigned short int const *>( instruction ) };
    instruction += 2;
    return result;
}

std::string r_m_name( unsigned char const r_m,
                      unsigned char const mod,
                      unsigned char const w,
                      unsigned char const *& instruction ) {
    if ( mod == 0b11 )
        return reg_names[(w << 3) | r_m];

    // Special case
    bool const direct_address { mod == 0b00 and r_m == 0b110 };

    int displacement { 0 };
    if ( mod == 0b01 )
        displacement = signed_byte_value( instruction );
    else if ( mod == 0b10 || direct_address )
        displacement = signed_word_value( instruction );

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


bool decode_move_register( unsigned char const *& instruction ) {
    struct Instruction {
        // Order of attributes within a single byte is reverse w.r.t. (my) mental view on bit ordering
        unsigned char w : 1;
        unsigned char d : 1;
        unsigned char opcode : 6;

        unsigned char r_m : 3;
        unsigned char reg : 3;
        unsigned char mod : 2;
    };
    auto const values { reinterpret_cast<Instruction const *>(instruction) };
    instruction += 2;

    if ( values->opcode != 0b100010 ) // This shouldn't happen...
        return false;

    std::string source { reg_names[(values->w << 3) | values->reg] };
    std::string destination { r_m_name( values->r_m, values->mod, values->w, instruction ) };
    if ( values->d )
        std::swap( source, destination );

    std::println( "mov {}, {}", destination, source );
    return true;
}

bool decode_move_immediate( unsigned char const *& instruction ) {
    struct Instruction {
        unsigned char reg : 3;
        unsigned char w : 1;
        unsigned char opcode : 4;
    };
    auto const values { reinterpret_cast<Instruction const *>(instruction) };
    ++instruction;

    if ( values->opcode != 0b1011 )
        return false;

    unsigned char const destination ( (values->w << 3) | values->reg );
    int const source ( values->w ? signed_word_value( instruction ) : signed_byte_value( instruction ) );

    std::println( "mov {}, {}", reg_names[destination], source );
    return true;
}

bool decode_move_immediate_memory( unsigned char const *& instruction ) {
    struct Instruction {
        unsigned char w : 1;
        unsigned char opcode : 7;
        unsigned char r_m : 3;
        unsigned char empty : 3;
        unsigned char mod : 2;
    };
    auto const values { reinterpret_cast<Instruction const *>(instruction) };
    instruction += 2;

    if ( values->opcode != 0b1100011 )
        return false;
    if ( values->empty != 0b000 )
        return false;

    std::string const destination { r_m_name( values->r_m, values->mod, values->w, instruction ) };
    std::string const source_type { values->w ? "word" : "byte" };
    int const source_value { (values->w ? signed_word_value : signed_byte_value)( instruction ) };

    std::println( "mov {}, {} {}", destination, source_type, source_value );
    return true;
}


bool decode_move_accumulator( unsigned char const *& instruction ) {
    struct Instruction {
        unsigned char w : 1;
        unsigned char d : 1;
        unsigned char opcode : 6;
    };
    auto const values { reinterpret_cast<Instruction const *>(instruction) };
    ++instruction;

    if ( values->opcode != 0b101000 )
        return false;

    int source_value { (values->w ? unsigned_word_value : unsigned_byte_value)( instruction ) };
    std::string source { std::format( "[{}]", source_value ) };
    std::string destination { "ax" };
    if ( values->d )
        std::swap( source, destination );

    std::println( "mov {}, {}", destination, source );
    return true;
}


std::array<std::function<bool(unsigned char const *&)>, 256> constexpr decoding_table() {
    std::array<std::function<bool(unsigned char const *&)>, 256> table { nullptr };

    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        table[0b1000'1000 | i] = decode_move_register;
    for ( unsigned char i { 0 }; i < (1 << 4); ++i )
        table[0b1011'0000 | i] = decode_move_immediate;
    for ( unsigned char i { 0 }; i < (1 << 1); ++i )
        table[0b1100'0110 | i] = decode_move_immediate_memory;
    for ( unsigned char i { 0 }; i < (1 << 2); ++i )
        table[0b1010'0000 | i] = decode_move_accumulator;

    return table;
};


bool decode( unsigned char const *& instruction ) {
    static auto const table { decoding_table() };

    auto const & decoder { table[*instruction] };
    if ( decoder )
        return decoder( instruction );

    std::println( "Unsupported instruction {:#04x}.", *instruction );
    return false;
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

    unsigned char const * instruction { instructions };
    while ( instruction != instructions + size ) {
        if ( not decode( instruction ) ) {
            std::println( "... Error while decoding instruction." );
            return 1;
        }
    }

    delete[] instructions;
    return 0;
}
