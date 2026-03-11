#pragma once

#include "instruction.hpp"

#include <ostream>
#include <vector>


class Simulator {
public:
    Simulator();

    void execute( Instruction const & instruction );
    void execute( std::vector<Instruction> const & instructions );

    unsigned int get( Operand const & memory ) const;
    unsigned int get( Register const & memory ) const;
    unsigned int get( SegmentRegister const & memory ) const;

    void set( Operand const & memory, unsigned int value );
    void set( Register const & memory, unsigned int value );
    void set( SegmentRegister const & memory, unsigned int value );

    friend std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs );

private:
    unsigned short int m_registers[12];
};


FORMATTER( Simulator )
