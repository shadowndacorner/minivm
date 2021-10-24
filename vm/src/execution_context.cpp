#include <minivm/vm.hpp>

namespace minivm
{
    execution_context::execution_context(program& program) : _program(program)
    {
    }

    const char* execution_context::get_error()
    {
        if (_error.size() == 0) return 0;
        return _error.c_str();
    }

    bool execution_context::run_from(const std::string_view& label)
    {
        // TODO: Do this without temporary allocation (we should be using a
        // string table rather than a map of strings)

        std::string temp(label);
        if (!_program.labels.count(temp))
        {
            _error = "Unknown label " + temp;
            return false;
        }

        _registers.pc = _program.labels[temp];
        run();
        return true;
    }

    void execution_context::run()
    {
        bool shouldRun = true;
        size_t pSize = _program.opcodes.size();
        while (shouldRun && _registers.pc < pSize)
        {
            auto& code = _program.opcodes[_registers.pc];
            switch (code.instruction)
            {
                case instruction::loadic:
                    _program.constants[code.warg0].get_i64(
                        _registers.registers[code.arg2].ireg);
                    break;
                case instruction::loaduc:
                    _program.constants[code.warg0].get_u64(
                        _registers.registers[code.arg2].ureg);
                    break;
                case instruction::loadfc:
                    _program.constants[code.warg0].get_f64(
                        _registers.registers[code.arg2].freg);
                    break;
                case instruction::addi:
                    _registers.registers[code.arg0].ireg +=
                        _registers.registers[code.arg1].ireg;
                    break;
                case instruction::addu:
                    _registers.registers[code.arg0].ureg +=
                        _registers.registers[code.arg1].ureg;
                    break;
                case instruction::addf:
                    _registers.registers[code.arg0].freg +=
                        _registers.registers[code.arg1].freg;
                    break;
                case instruction::printi:
                    printf("%zd\n", _registers.registers[code.arg0].ireg);
                    break;
                case instruction::printu:
                    printf("%zu\n", _registers.registers[code.arg0].ureg);
                    break;
                case instruction::printf:
                    printf("%f\n", _registers.registers[code.arg0].freg);
                    break;
                case instruction::yield:
                    shouldRun = false;
                    break;
                case instruction::Count:
                    break;
            }
            ++_registers.pc;
        }
    }

}  // namespace minivm