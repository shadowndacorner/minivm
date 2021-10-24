#include <minivm/vm.hpp>

namespace minivm
{
    execution_context::execution_context(program& program)
        : _program(program), _did_yield(false)
    {
    }

    const char* execution_context::get_error()
    {
        if (_error.size() == 0) return 0;
        return _error.c_str();
    }

    bool execution_context::jump(const std::string& label)
    {
        if (!_program.labels.count(label))
        {
            _error = "Unknown label " + label;
            return false;
        }

        _registers.pc = _program.labels[label];
        return true;
    }

    bool execution_context::run_from(const std::string_view& label)
    {
        if (!jump(std::string(label))) return false;
        return run();
    }

    bool execution_context::resume()
    {
        return run();
    }

    bool execution_context::did_yield() const
    {
        return _did_yield;
    }

    bool execution_context::run()
    {
        _did_yield = false;
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
                case instruction::loadii:
                    _registers.registers[code.arg0].ireg =
                        _registers.registers[code.arg1].ireg;
                    break;
                case instruction::loaduu:
                    _registers.registers[code.arg0].ureg =
                        _registers.registers[code.arg1].ureg;
                    break;
                case instruction::loadff:
                    _registers.registers[code.arg0].freg =
                        _registers.registers[code.arg1].freg;
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
                case instruction::subi:
                    _registers.registers[code.arg0].ireg -=
                        _registers.registers[code.arg1].ireg;
                    break;
                case instruction::subu:
                    _registers.registers[code.arg0].ureg -=
                        _registers.registers[code.arg1].ureg;
                    break;
                case instruction::subf:
                    _registers.registers[code.arg0].freg -=
                        _registers.registers[code.arg1].freg;
                    break;
                case instruction::muli:
                    _registers.registers[code.arg0].ireg *=
                        _registers.registers[code.arg1].ireg;
                    break;
                case instruction::mulu:
                    _registers.registers[code.arg0].ureg *=
                        _registers.registers[code.arg1].ureg;
                    break;
                case instruction::mulf:
                    _registers.registers[code.arg0].freg *=
                        _registers.registers[code.arg1].freg;
                    break;
                case instruction::divi:
                    _registers.registers[code.arg0].ireg /=
                        _registers.registers[code.arg1].ireg;
                    break;
                case instruction::divu:
                    _registers.registers[code.arg0].ureg /=
                        _registers.registers[code.arg1].ureg;
                    break;
                case instruction::divf:
                    _registers.registers[code.arg0].freg /=
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
                case instruction::printsc:
                    printf("%s\n",
                           _program.constants[code.warg0].string_ref().c_str());
                    break;
                case instruction::yield:
                    _did_yield = true;
                    shouldRun = false;
                    break;
                case instruction::cmp:
                    _registers.cmp = _registers.registers[code.arg1].ireg -
                                     _registers.registers[code.arg0].ireg;
                    break;
                case instruction::jump:
                    // Subtracting 1 because we increment pc after
                    _registers.pc = code.warg0 - 1;
                    break;
                case instruction::jeq:
                    if (!_registers.cmp)
                    {
                        // Subtracting 1 because we increment pc after
                        _registers.pc = code.warg0 - 1;
                    }
                    break;
                case instruction::jne:
                    if (_registers.cmp)
                    {
                        // Subtracting 1 because we increment pc after
                        _registers.pc = code.warg0 - 1;
                    }
                    break;
                case instruction::Count:
                    break;
            }
            ++_registers.pc;
        }
        return true;
    }

}  // namespace minivm