#pragma once
#include <stdint.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace minivm
{
    enum class instruction : uint8_t
    {
        loadc,

        stores,
        loads,

        // loads
        loadi,
        loadu,
        loadf,

        // conversion
        utoi,
        utof,
        itou,
        itof,
        ftoi,
        ftou,

        // arithmetic
        addi,
        addu,
        addf,
        subi,
        subu,
        subf,
        muli,
        mulu,
        mulf,
        divi,
        divu,
        divf,

        // debug
        printi,
        printu,
        printf,
        prints,

        // control flow
        cmp,
        jump,
        jeq,
        jne,

        // execution
        yield,
        ret,

        Count
    };

    struct opcode
    {
        union
        {
            struct
            {
                uint8_t reg0 : 4;
                uint8_t reg1 : 4;
                uint8_t reg2 : 4;
                uint8_t reg3 : 4;
            };
            uint32_t warg0;
        };
        uint16_t sarg1;
        instruction instruction;
    };

    struct vm_register
    {
        union
        {
            int64_t ireg;
            uint64_t ureg;
            double freg;
        };
    };

    struct constant_value
    {
        constant_value();

        vm_register value;
        bool is_data_offset;
        bool is_pointer;

        inline void set(uint64_t val)
        {
            value.ureg = val;
        }

        inline void set(int64_t val)
        {
            value.ireg = val;
        }

        inline void set(double val)
        {
            value.freg = val;
        }
    };

    class program
    {
        friend class asm_parser;
        friend class execution_context;

    public:
        bool load_assembly(const std::string_view& mvmaSrc);
        bool load_assembly_from_file(const std::string_view& filename);
        const char* get_load_error();

    private:
        std::string load_error;
        std::vector<char> _data;
        std::vector<constant_value> constants;
        std::vector<opcode> opcodes;
        std::unordered_map<std::string, uint32_t> labels;
    };

    struct vm_execution_registers
    {
        vm_register registers[16];
        uint32_t pc;
        uint32_t cmp;
        uint32_t sp;
    };

    struct stack_frame
    {
        uint32_t label;
        uint32_t pc;
        uint32_t stack_baseptr;
    };

    class execution_context
    {
    public:
        execution_context(program& program);

    public:
        const char* get_error();
        bool run_from(const std::string_view& label);
        bool resume();
        bool did_yield() const;

    private:
        bool run();
        bool jump(const std::string& label);

    private:
        vm_execution_registers _registers;
        std::vector<stack_frame> _callStack;
        std::vector<uint8_t> _stack;
        program& _program;
        std::string _error;
        bool _did_yield;
    };
}  // namespace minivm