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
        // stack manip
        salloc,
        push,
        pop,

        stores,
        dstores,
        loads,
        dloads,

        // loads
        loadic,
        loaduc,
        loadfc,
        loadii,
        loaduu,
        loadff,

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
        printsc,

        // execution
        yield,
        cmp,
        jump,
        jeq,
        jne,
        Count
    };

    struct opcode
    {
        union
        {
            struct
            {
                uint32_t warg0;
                union
                {
                    uint16_t arg0;
                    int16_t sarg0;
                };
                union
                {
                    uint16_t arg1;
                    int16_t sarg1;
                };
            };
        };

        union
        {
            uint16_t arg2;
            int16_t sarg2;
        };

        instruction instruction;
        uint8_t reg0;
    };

    struct constant_value
    {
        std::string& string_ref();
        bool get_string(std::string_view&);
        bool get_i64(int64_t&);
        bool get_u64(uint64_t&);
        bool get_f64(double&);

        inline void set(int64_t value)
        {
            _value = value;
        }

        inline void set(uint64_t value)
        {
            _value = value;
        }

        inline void set(double value)
        {
            _value = value;
        }

        inline void set(const std::string_view& value)
        {
            _value = std::string(value);
        }

        inline void move(std::string&& value)
        {
            auto& val = std::get<std::string>(_value);
            val.swap(value);
        }

    private:
        std::variant<std::string, int64_t, uint64_t, double> _value;
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
        std::vector<constant_value> constants;
        std::vector<opcode> opcodes;
        std::unordered_map<std::string, uint32_t> labels;
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

    struct vm_execution_registers
    {
        vm_register registers[16];
        uint32_t pc;
        uint32_t cmp;
        uint32_t sp;
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
        std::vector<uint64_t> _stack;
        program& _program;
        std::string _error;
        bool _did_yield;
    };
}  // namespace minivm