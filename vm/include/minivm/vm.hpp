#pragma once
#include <stdint.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace minivm
{
    enum class instruction : uint16_t
    {
        loadic,
        loaduc,
        loadfc,
        addi,
        addu,
        addf,
        printi,
        printu,
        printf,
        yield,
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
    };

    struct constant_value
    {
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
        uint64_t pc;
    };

    class execution_context
    {
    public:
        execution_context(program& program);

    public:
        const char* get_error();
        bool run_from(const std::string_view& label);

    private:
        void run();

    private:
        vm_execution_registers _registers;
        program& _program;
        std::string _error;
    };
}  // namespace minivm