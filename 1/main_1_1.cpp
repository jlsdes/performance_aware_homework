#include <array>
#include <filesystem>
#include <fstream>
#include <print>


// Register field encoding, where indices are formed by 4 bits: <w><reg>
std::array<char const *, 16> constexpr reg_names {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
};


struct Mov {
    // Order of attributes within a single byte is reverse w.r.t. (my) mental view on bit ordering
    unsigned char w : 1;
    unsigned char d : 1;
    unsigned char opcode : 6;

    unsigned char r_m : 3;
    unsigned char reg : 3;
    unsigned char mod : 2;
};


bool decode( unsigned char const *& instruction ) {
    auto const move { reinterpret_cast<Mov const *>(instruction) };

    if ( move->opcode != 0b100010 ) { // Only allow mov instructions for now
        std::println( "Unsupported opcode {:#4x}.", move->opcode );
        return false;
    }
    if ( move->mod != 0b11 ) { // Only allow register mode for now
        std::println( "Unsupported mod {:#4x}.", move->mod );
        return false;
    }

    unsigned char source ( (move->w << 3) | move->reg );
    unsigned char destination ( (move->w << 3) | move->r_m );
    if ( move->d )
        std::swap( source, destination );

    std::println( "mov {}, {}", reg_names[destination], reg_names[source] );

    instruction += 2;
    return true;
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
    for ( unsigned int i { 0 }; i < (size >> 1); ++i ) {
        if ( not decode( instruction ) )
            return 1;
    }

    delete[] instructions;
    return 0;
}
