#include <filesystem>
#include <fstream>
#include <ios>
#include <print>


int main( int const argc, char const * const * const argv ) {
    if ( argc != 3 ) {
        std::println( "Invalid number of arguments, expected 2 files." );
        return 1;
    }

    std::filesystem::path paths[2];
    std::ifstream files[2];

    char const * const * filename { argv + 1 };
    for ( int i { 0 }; i < 2; ++i ) {
        paths[i] = std::filesystem::path { *filename };
        if ( not std::filesystem::exists( paths[i] ) ) {
            std::println( "File {} does not exist.", *filename );
            return 1;
        }
        files[i] = std::ifstream { paths[i], std::ios::binary };
        if ( not files[i].is_open() ) {
            std::println( "File {} could not be opened.", *filename );
            return 1;
        }
        ++filename;
    }

    std::size_t const size[2] { std::filesystem::file_size( paths[0] ), std::filesystem::file_size( paths[1] ) };
    if ( size[0] != size[1] )
        std::println( "Files have different sizes." );

    std::size_t const read_until { std::min( size[0], size[1] ) };
    char bytes[2];

    for ( unsigned int i { 0 }; i < read_until; ++i ) {
        files[0] >> bytes[0];
        files[1] >> bytes[1];

        if ( bytes[0] != bytes[1] )
            std::println( "Bytes {} have different values ({:#04x}, {:#04x}).", i, bytes[0], bytes[1] );
    }
    return 0;
}

