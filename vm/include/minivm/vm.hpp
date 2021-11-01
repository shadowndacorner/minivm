#pragma once
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace minivm
{
    enum class instruction : uint8_t
    {
        // Constants
        loadc,

        // Externals
        eload,
        estore,

        // Stack frame stores
        sstore,
        sstoreu32,
        sstoreu16,
        sstoreu8,
        sstorei32,
        sstorei16,
        sstorei8,
        sstoref32,

        // Stack frame loads
        sload,
        sloadu32,
        sloadu16,
        sloadu8,
        sloadi32,
        sloadi16,
        sloadi8,
        sloadf32,

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

        // register manipulation
        mov,
        utoi,
        utof,
        itou,
        itof,
        ftoi,
        ftou,

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
        callext,
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

    struct vm_word_t
    {
        union
        {
            int64_t ireg;
            uint64_t ureg;
            double freg;
        };
    };

    struct program_extern_value
    {
        vm_word_t value;
    };

    struct constant_value
    {
        constant_value();

        vm_word_t value;
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

    struct program_label_id_t
    {
        program_label_id_t() = default;
        program_label_id_t(const program_label_id_t&) = default;
        program_label_id_t& operator=(const program_label_id_t&) = default;

        inline program_label_id_t(uint32_t id) : idx(id) {}

        uint32_t idx;
    };

    struct program_extern_id_t
    {
        program_extern_id_t() = default;
        program_extern_id_t(const program_extern_id_t&) = default;
        program_extern_id_t& operator=(const program_extern_id_t&) = default;

        inline program_extern_id_t(uint32_t id) : idx(id) {}

        uint32_t idx;
    };

    struct vm_execution_registers
    {
        vm_word_t registers[16];
        uint32_t pc;
        uint32_t cmp;
        uint32_t sp;
    };

    typedef void (*extern_program_func_t)(vm_execution_registers* registers);

    class program
    {
        friend class asm_parser;
        friend class execution_context;

    public:
        bool load_assembly(const std::string_view& mvmaSrc);
        bool load_assembly_from_file(const std::string_view& filename);
        const char* get_load_error();

    public:
        template <typename T>
        inline bool set_extern_pointer(const std::string_view& name, T* ptr)
        {
            return set_unsigned_extern(name, reinterpret_cast<size_t>(ptr));
        }

        bool set_extern_function_ptr(const std::string_view& name,
                                     extern_program_func_t func);

        bool set_unsigned_extern(const std::string_view& name, uint64_t value);
        bool set_signed_extern(const std::string_view& name, int64_t value);
        bool set_floating_extern(const std::string_view& name, double value);

        bool get_extern_ptr(const std::string_view& name, uint64_t** value);

        bool get_extern_ptr(const std::string_view& name, int64_t** value);

        bool get_extern_ptr(const std::string_view& name, double** value);

    private:
        uint32_t write_static_string(const std::string_view& string);

    private:
        program_label_id_t get_label_id(const std::string_view& label);
        program_label& get_label(const std::string_view& label);
        program_label& get_label(program_label_id_t);

    private:
        program_extern_id_t get_extern_id(const std::string_view& label);
        program_extern_value& get_extern(const std::string_view& label);
        program_extern_value& get_extern(program_extern_id_t);

    private:
        std::string load_error;
        std::vector<char> _data;
        std::vector<constant_value> constants;
        std::vector<opcode> opcodes;
        std::unordered_map<std::string, program_label_id_t> label_map;
        std::vector<program_label> labels;
        std::unordered_map<std::string, program_extern_id_t> extern_map;
        std::vector<program_extern_value> externs;
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
        void call(program_label_id_t label);
        void jump(program_label_id_t label);
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