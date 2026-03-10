#pragma once

#include "instruction.hpp"

#include <format>
#include <ostream>
#include <sstream>


class Simulator {
public:
    Simulator();

    void execute( Instruction const & instruction );

    friend std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs );

private:
    short int m_registers[8];
};


template <>
struct std::formatter<Simulator> : std::formatter<std::string> {
    inline auto format( Simulator const & simulator, format_context & context ) const {
        std::stringstream stream {};
        stream << simulator;
        return std::formatter<std::string>::format( std::format( "{}", stream.str() ), context );
    }
};
