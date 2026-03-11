#include "simulator.hpp"
#include "instruction.hpp"

#include <format>
#include <print>
#include <stdexcept>
#include <variant>


Simulator::Simulator() : m_registers {} {}


void Simulator::execute( Instruction const & instruction ) {
    std::print( "{:40}; ", instruction );

    if ( instruction.name == "mov" ) {
        if ( not std::holds_alternative<Register>( instruction.operands[0] ) )
            throw std::invalid_argument( "Unsupported destination operand." );
        if ( not std::holds_alternative<Immediate>( instruction.operands[1] ) )
            throw std::invalid_argument( "Unsupported source operand." );

        Register const destination { std::get<RegisterOperand>( instruction.operands[0] ) };
        Immediate const source { std::get<ImmediateOperand>( instruction.operands[1] ) };

        std::print( "{:#04x} -> ", get( destination ) );
        set( destination, source.value );
        std::println( "{:#04x}", get( destination ) );

    } else
        throw std::invalid_argument( std::format( "Unsupported instruction: {}", instruction.name ) );
}


void Simulator::execute( std::vector<Instruction> const & instructions ) {
    for ( Instruction const & instruction : instructions )
        execute( instruction );
}


static int get( RegisterMemory const & registers, Register const & reg ) {
    if ( reg.w )
        return registers[reg.base];
    else
        return reinterpret_cast<char const *>( registers )[reg.base];
}


static void set( RegisterMemory & registers, Register const & reg, int const value ) {
    if ( reg.w )
        registers[reg.base] = value;
    else
        reinterpret_cast<char *>( registers )[reg.base] = value;
}


int Simulator::get( Operand const & source ) const {
    switch ( source.index() ) {
    case RegisterOperand:
        return ::get( m_registers, std::get<RegisterOperand>( source ) );
    default:
        throw std::invalid_argument( "Unsupported source operand." );
    }
}


void Simulator::set( Operand const & destination, int const value ) {
    switch ( destination.index() ) {
    case RegisterOperand:
        return ::set( m_registers, std::get<RegisterOperand>( destination ), value );
    default:
        throw std::invalid_argument( "Unsupported destination operand." );
    }
}
