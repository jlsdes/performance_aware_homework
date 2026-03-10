#include "simulator.hpp"
#include "instruction.hpp"

#include <ios>


Simulator::Simulator() : m_registers {} {}


void Simulator::execute( Instruction const & instruction ) {
}


std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs ) {
    lhs << "Registers:\n";
    for ( unsigned int i { 0 }; i < 8; ++i ) {
        lhs << '\t' << reg_names[0b1000 | i] << ": ";
        lhs << std::hex << rhs.m_registers[i];
        lhs << std::dec << "\t( " << rhs.m_registers[i] << " )\n";
    }
    return lhs;
}
