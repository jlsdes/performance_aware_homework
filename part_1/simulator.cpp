#include "simulator.hpp"
#include "instruction.hpp"

#include <format>
#include <print>
#include <stdexcept>


static unsigned char constexpr byte_offsets[8] {
    RegAX * 2, // AL
    RegCX * 2, // CL
    RegDX * 2, // DL
    RegBX * 2, // BL
    RegAX * 2 + 1, // AH
    RegCX * 2 + 1, // CH
    RegDX * 2 + 1, // DH
    RegBX * 2 + 1, // BH
};


Simulator::Simulator() : m_registers {} {} 


void Simulator::execute( Instruction const & instruction ) {
    std::print( "{:40}; ", instruction );

    if ( instruction.name == "mov" )
        set( instruction.operands[0], get( instruction.operands[1] ) );
    else
        throw std::invalid_argument( std::format( "Unsupported instruction: {}", instruction.name ) );
}


void Simulator::execute( std::vector<Instruction> const & instructions ) {
    for ( Instruction const & instruction : instructions )
        execute( instruction );
}


unsigned int Simulator::get( Operand const & memory ) const {
    switch ( memory.index() ) {
    case RegisterOperand:   return get( std::get<RegisterOperand>( memory ) );
    case SegRegOperand:     return get( std::get<SegRegOperand>( memory ) ); 
    case ImmediateOperand:  return std::get<ImmediateOperand>( memory ).value;
    default:
        throw std::invalid_argument( "Unsupported source operand." );
    }
}


unsigned int Simulator::get( Register const & memory ) const {
    if ( memory.w )
        return m_registers[memory.base];
    else
        return reinterpret_cast<char const *>( m_registers )[byte_offsets[memory.base]];
}


unsigned int Simulator::get( SegmentRegister const & memory ) const {
    return m_registers[8 + memory.base];
}


void Simulator::set( Operand const & memory, unsigned int const value ) {
    switch ( memory.index() ) {
    case RegisterOperand:   set( std::get<RegisterOperand>( memory ), value );  break;
    case SegRegOperand:     set( std::get<SegRegOperand>( memory ), value );    break;
    default:
        throw std::invalid_argument( "Unsupported destination operand." );
    }
}


void Simulator::set( Register const & memory, unsigned int const value ) {
    if ( memory.w ) {
        std::println( "{:#06x} -> {:#06x}", m_registers[memory.base], value );
        m_registers[memory.base] = value;
    } else {
        auto & target { reinterpret_cast<unsigned char *>( m_registers )[byte_offsets[memory.base]] };
        std::println( "{:#04x} -> {:#04x}", target, value );
        target = value;
    }
}


void Simulator::set( SegmentRegister const & memory, unsigned int const value ) {
    std::println( "{:#06x} -> {:#06x}", m_registers[8 + memory.base], value );
    m_registers[8 + memory.base] = value;
}


std::ostream & operator<<( std::ostream & lhs, Simulator const & rhs ) {
    std::println( lhs, "Registers:" );
    for ( unsigned char i { 0 }; i < 8; ++i ) {
        auto reg { rhs.get( Register { i, 1 } ) };
        std::println( lhs, "\t{}: {:#06x}\t({})", reg_names[0x8 | i], reg, reg );
    }
    std::println( lhs, "Segment registers:" );
    for ( unsigned char i { 0 }; i < 4; ++i ) {
        auto seg_reg { rhs.get( SegmentRegister { i } ) };
        std::println( lhs, "\t{}: {:#06x}\t({})", seg_reg_names[i], seg_reg, seg_reg );
    }
    return lhs;
}

