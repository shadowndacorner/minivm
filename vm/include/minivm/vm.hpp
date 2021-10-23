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
        load,
        add,
        printi,
        jump,
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
        bool as_f64(double&);

        template <typename T>
        inline void set(const T& value)
        {
            _value = value;
        }

    private:
        std::variant<std::string, int64_t, uint64_t, double> _value;
    };

    class program
    {
        friend class asm_parser;

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
            double dreg;
        };
    };

    struct vm_execution_registers
    {
        vm_register registers[16];
    };

    class execution_context
    {
    public:
        execution_context(program& program);

    private:
    };
}  // namespace minivm