#include "decoder.hpp"
#include "simulator.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <print>


int main( int const argc, char const * const * const argv ) {
    if ( argc < 2 or argc > 3 ) {
        std::println( "Invalid number of arguments, expected '<action> <file>' or '-help'" );
        return 1;
    }

    std::string const action { argv[1] };
    std::string const filename { argv[2] };

    if ( action == "-help" ) {
        std::println( "Available actions are:" );
        std::println( " -help : shows this listing" );
        std::println( " -decode : decodes a single file and prints the disassembly" );
        std::println( " -exec : decodes a single file and simulates the program" );
        return 0;
    }

    std::filesystem::path const path { filename };
    if ( not std::filesystem::exists( path ) ) {
        std::println( "File {} does not exist.", filename );
        return 1;
    }

    std::ifstream file { path, std::ios_base::binary };
    if ( not file.is_open() ) {
        std::println( "Failed to open file {}.", filename );
        return 1;
    }

    std::size_t const size { std::filesystem::file_size( path ) };
    unsigned char * const instructions { new unsigned char[size] {} };
    file.read( reinterpret_cast<char *>(instructions), size );

    std::println( "; Disassembly for {}", filename );
    std::println( "bits 16");
    std::println();

    decode_all( instructions, instructions + size, true );

    if ( action == "-exec" ) {
        Simulator simulator {};

        std::println();
        std::println( "State after the simulation:" );
        std::println( "{}", simulator );
    }

    delete[] instructions;
    return 0;
}
