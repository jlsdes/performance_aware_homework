#pragma once

#include "instruction.hpp"

#include <ostream>
#include <vector>


class Simulator {
public:
    Simulator();

    void execute( Instruction const & instruction );
    void execute( std::vector<Instruction> const & instructions );

    int get( Operand const & source ) const;
    int get( Register const & source ) const;

    void set( Operand const & destination, int value );
    void set( Register const & destination, int value );

    friend std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs );

private:
    short int m_registers[8];
};


FORMATTER( Simulator )
