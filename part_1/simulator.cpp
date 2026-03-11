#include "simulator.hpp"
#include "instruction.hpp"

#include <format>
#include <print>
#include <stdexcept>


Simulator::Simulator() : m_registers {} {}


void Simulator::execute( Instruction const & instruction ) {
    std::print( "{:40}; ", instruction );

    if ( instruction.name == "mov" ) {
        Operand const destination { instruction.operands[0] };
        Operand const source { instruction.operands[1] };

        std::print( "{:#04x} ->", get( destination ) );
        set( destination, get( source ) );
        std::println( " {:#04x}", get( destination ) );
    } else
        throw std::invalid_argument( std::format( "Unsupported instruction: {}", instruction.name ) );
}


void Simulator::execute( std::vector<Instruction> const & instructions ) {
    for ( Instruction const & instruction : instructions )
        execute( instruction );
}


int Simulator::get( Operand const & source ) const {
    switch ( source.index() ) {
    case RegisterOperand:
        return get( std::get<RegisterOperand>( source ) );
    case ImmediateOperand:
        return std::get<ImmediateOperand>( source ).value;
    default:
        throw std::invalid_argument( "Unsupported source operand." );
    }
}


int Simulator::get( Register const & reg ) const {
    if ( reg.w )
        return m_registers[reg.base];
    else
        return reinterpret_cast<char const *>( m_registers )[reg.base];
}


void Simulator::set( Operand const & destination, int const value ) {
    switch ( destination.index() ) {
    case RegisterOperand:
        return set( std::get<RegisterOperand>( destination ), value );
    default:
        throw std::invalid_argument( "Unsupported destination operand." );
    }
}


void Simulator::set( Register const & reg, int const value ) {
    if ( reg.w )
        m_registers[reg.base] = value;
    else
        reinterpret_cast<char *>( m_registers )[reg.base] = value;
}


std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs ) {
    lhs << "Registers:\n";
    for ( unsigned int i { 0 }; i < 8; ++i ) {
        auto const reg { rhs.get( Register { static_cast<unsigned char>( i ), 1 } ) };
        std::println( lhs, "\t{}: {:#06x}\t({})", reg_names[0x8 | i], reg, reg );
    }
    return lhs;
}

