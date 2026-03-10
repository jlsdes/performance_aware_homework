#pragma once

#include "instruction.hpp"

#include <ostream>
#include <vector>


class Simulator {
public:
    Simulator();

    void execute( Instruction const & instruction );
    void execute( std::vector<Instruction> const & instructions );

    int get( Register const & reg ) const;
    void set( Register const & reg, int value );

    friend std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs );

private:
    short int m_registers[8];
};


inline std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs ) {
    lhs << "Registers:\n";
    for ( unsigned int i { 0 }; i < 8; ++i )
        std::println( lhs, "\t{}: {:#06x}\t({})", reg_names[0x8 | i], rhs.m_registers[i], rhs.m_registers[i] );
    return lhs;
}


FORMATTER( Simulator )
