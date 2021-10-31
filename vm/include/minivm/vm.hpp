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
        call,
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
        uint16_t arg1;
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

    struct program_label
    {
        union
        {
            uint64_t offset;
            const char* name;
        };

        uint32_t pc;
        uint32_t stackalloc;
    };

    struct program_label_id
    {
        program_label_id() = default;
        program_label_id(const program_label_id&) = default;
        program_label_id& operator=(const program_label_id&) = default;

        inline program_label_id(uint32_t id) : idx(id) {}

        uint32_t idx;
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
        uint32_t write_static_string(const std::string_view& string);
        program_label_id get_label_id(const std::string_view& label);
        program_label& get_label(const std::string_view& label);
        program_label& get_label(program_label_id);

    private:
        std::string load_error;
        std::vector<char> _data;
        std::vector<constant_value> constants;
        std::vector<opcode> opcodes;
        std::unordered_map<std::string, program_label_id> label_map;
        std::vector<program_label> labels;
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
        // This is more expensive than it needs to be, but it's a simple way of
        // doing this.
        vm_execution_registers state;
        uint32_t label;
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
        void call(program_label_id label);
        void jump(program_label_id label);
        void jump(const program_label& label);

    private:
        vm_execution_registers _registers;
        std::vector<stack_frame> _callStack;
        std::vector<uint8_t> _stack;
        program& _program;
        std::string _error;
        bool _did_yield;
    };
}  // namespace minivm