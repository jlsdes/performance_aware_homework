#pragma once

#include <array>
#include <ostream>
#include <string>
#include <sstream>
#include <variant>


struct Register {
    unsigned char base : 3; // The final two identifier bits.
    unsigned char w : 1; // Whether the register is wide (true) or not (false).
    bool empty : 1; // Whether the register value is empty, only used for EffectiveAddress stuff.
};


Register constexpr empty_register { 0, 0, true };


enum RegName : unsigned char {
    RegA,
    RegC,
    RegD,
    RegB,
    RegSP,
    RegBP,
    RegSI,
    RegDI
};


struct SegmentRegister {
    unsigned char base : 2;
};


enum SegRegName : unsigned char {
    SegRegES,
    SegRegCS,
    SegRegSS,
    SegRegDS,
};


struct EffectiveAddress {
    Register regs[2];
    int offset;
};


struct Immediate {
    int value;
    bool wide;
};


using None = std::nullptr_t;

using Operand = std::variant<None, Register, SegmentRegister, EffectiveAddress, Immediate>;

enum OperandTypes {
    NoOperand,
    RegisterOperand,
    SegRegOperand,
    AddressOperand,
    ImmediateOperand,
};


struct Instruction {
    unsigned char const * bytes;

    std::string name;
    Operand operands[2];

    bool write_size { false };
};


template <std::size_t table_size>
using StringTable = std::array<std::string, table_size>;


// Register field encoding, where indices are formed by 4 bits: <w><reg>
StringTable<16> constexpr reg_names {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
};


// Base effective address calculations
StringTable<8> constexpr reg_sums {
    "bx + si",
    "bx + di",
    "bp + si",
    "bp + di",
    "si",
    "di",
    "bp",
    "bx",
};


EffectiveAddress constexpr address_sums[8] {
    { { { RegB, 1 }, { RegSI, 1 } } },
    { { { RegB, 1 }, { RegDI, 1 } } },
    { { { RegBP, 1 }, { RegSI, 1 } } },
    { { { RegBP, 1 }, { RegDI, 1 } } },
    { { { RegSI, 1 }, empty_register } },
    { { { RegDI, 1 }, empty_register } },
    { { { RegBP, 1 }, empty_register } },
    { { { RegB, 1 }, empty_register } },
};


inline EffectiveAddress constexpr direct_address( int const displacement ) {
    return { .regs = { empty_register, empty_register }, .offset = displacement };
}


// Segment register names
StringTable<4> constexpr seg_reg_names {
    "es",
    "cs",
    "ss",
    "ds",
};


// Based on page 178 of https://edge.edx.org/c4x/BITSPilani/EEE231/asset/8086_family_Users_Manual_1_.pdf
StringTable<256> constexpr mnemonics {
    "add", "add", "add", "add", "add", "add", "push", "pop", "or", "or", "or", "or", "or", "or", "push", "ERR",
    "adc", "adc", "adc", "adc", "adc", "adc", "push", "pop", "sbb", "sbb", "sbb", "sbb", "sbb", "sbb", "push", "pop",
    "and", "and", "and", "and", "and", "and", "seg", "daa", "sub", "sub", "sub", "sub", "sub", "sub", "seg", "das",
    "xor", "xor", "xor", "xor", "xor", "xor", "seg", "aaa", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "seg", "aas",
    "inc", "inc", "inc", "inc", "inc", "inc", "inc", "inc", "dec", "dec", "dec", "dec", "dec", "dec", "dec", "dec",
    "push", "push", "push", "push", "push", "push", "push", "push", "pop", "pop", "pop", "pop", "pop", "pop", "pop", "pop",
    "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR", "ERR",
    "jo", "jno", "jb", "jnb", "je", "jne", "jbe", "jnbe", "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jnle",
    "IMM", "IMM", "IMM", "IMM", "test", "test", "xchg", "xchg", "mov", "mov", "mov", "mov", "mov", "lea", "mov", "pop",
    "xchg", "xchg", "xchg", "xchg", "xchg", "xchg", "xchg", "xchg", "cbw", "cwd", "call", "wait", "pushf", "popf", "sahf", "lahf",
    "mov", "mov", "mov", "mov", "movs", "movs", "cmps", "cmps", "test", "test", "stos", "stos", "lods", "lods", "scas", "scas",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "ERR", "ERR", "ret", "ret", "les", "lds", "mov", "mov", "ERR", "ERR", "retf", "retf", "int3", "int", "into", "iret",
    "SHIFT", "SHIFT", "SHIFT", "SHIFT", "aam", "aad", "ERR", "xlat", "esc", "esc", "esc", "esc", "esc", "esc", "esc", "esc",
    "loopnz", "loopz", "loop", "jcxz", "in", "in", "out", "out", "call", "jmp", "jmp", "jmp", "in", "in", "out", "out",
    "lock", "ERR", "rep", "rep", "hlt", "cmc", "GRP1", "GRP1", "clc", "stc", "cli", "sti", "cld", "std", "GRP2", "GRP2",
};


StringTable<8> constexpr immediate_table {
    "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp",
};


StringTable<8> constexpr shift_table {
    "rol", "ror", "rcl", "rcr", "shl", "shr", "ERR", "sar",
};


StringTable<8> constexpr group_1_table {
    "test", "ERR", "not", "neg", "mul", "imul", "div", "idiv",
};


StringTable<8> constexpr group_2_table {
    "inc", "dec", "call", "call", "jmp", "jmp", "push", "ERR",
};


/// Returns the mnemonic for the instruction.
inline std::string constexpr get_mnemonic( unsigned char const * const instruction ) {
    switch ( *instruction ) {
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
        return immediate_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    case 0xd0:
    case 0xd1:
    case 0xd2:
    case 0xd3:
        return shift_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    case 0xf6:
    case 0xf7:
        return group_1_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    case 0xfe:
    case 0xff:
        return group_2_table[(*(instruction + 1) >> 3) & 0b0000'0111];
    default:
        return mnemonics[*instruction];
    }
}


inline std::ostream & operator<<( std::ostream & lhs, Register const & rhs ) {
    // Ignoring rhs.empty; this should be checked by the caller.
    return lhs << reg_names[rhs.w << 3 | rhs.base];
}


inline std::ostream & operator<<( std::ostream & lhs, SegmentRegister const & rhs ) {
    return lhs << seg_reg_names[rhs.base];
}


inline std::ostream & operator<<( std::ostream & lhs, EffectiveAddress const & rhs ) {
    lhs << '[';

    if ( not rhs.regs[0].empty )
        lhs << rhs.regs[0];

    bool first = rhs.regs[0].empty;

    if ( not rhs.regs[1].empty ) {
        lhs << (first ? "" : " + ") << rhs.regs[1];
        first = false;
    }

    if ( first )
        lhs << rhs.offset;
    else if ( rhs.offset != 0 )
        lhs << (rhs.offset < 0 ? " - " : " + ") << std::abs( rhs.offset );

    return lhs << ']';
}


inline std::ostream & operator<<( std::ostream & lhs, Immediate const & rhs ) {
    return lhs << rhs.value;
}


inline std::ostream & operator<<( std::ostream & lhs, Instruction const & rhs ) {
    if ( rhs.name.empty() )
        lhs << get_mnemonic( rhs.bytes ) << ' ';
    else
        lhs << rhs.name << ' ';

    bool first { true };

    for ( unsigned int i { 0 }; i < 2; ++i ) {
        if ( std::holds_alternative<None>( rhs.operands[i] ))
            continue;

        if ( not first )
            lhs << ", ";
        first = false;

        if ( rhs.write_size ) {
            if ( i == 1 or std::holds_alternative<None>( rhs.operands[1] ) )
                lhs << ( *rhs.bytes & 1 ? "word " : "byte " );
        }

        switch ( rhs.operands[i].index() ) {
        case RegisterOperand:
            lhs << std::get<RegisterOperand>( rhs.operands[i] );
            break;

        case SegRegOperand:
            lhs << std::get<SegRegOperand>( rhs.operands[i] );
            break;

        case AddressOperand:
            lhs << std::get<AddressOperand>( rhs.operands[i] );
            break;

        case ImmediateOperand:
            lhs << std::get<ImmediateOperand>( rhs.operands[i] );
            break;

        default:
            throw std::exception();
        }
    }
    return lhs;
}


inline std::string to_string( Instruction const & instruction ) {
    std::stringstream stream {};
    stream << instruction;
    return stream.str();
}

