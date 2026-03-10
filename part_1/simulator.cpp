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


int Simulator::get( Register const & reg ) const {
    if ( reg.w )
        return m_registers[reg.base];
    else
        return reinterpret_cast<char const *>( m_registers )[reg.base];
}


void Simulator::set( Register const & reg, int const value ) {
    if ( reg.w )
        m_registers[reg.base] = value;
    else
        reinterpret_cast<char *>( m_registers )[reg.base] = value;
}
