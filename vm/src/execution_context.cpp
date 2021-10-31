#include <minivm/vm.hpp>

namespace minivm
{
    execution_context::execution_context(program& program)
        : _program(program), _did_yield(false)
    {
        _registers.sp = 0;
        _stack.reserve(4096);
    }

    const char* execution_context::get_error()
    {
        if (_error.size() == 0) return 0;
        return _error.c_str();
    }

    bool execution_context::run_from(const std::string_view& label)
    {
        std::string copy = std::string(label);
        if (!_program.label_map.count(copy))
        {
            _error = "Unknown label " + copy;
            return false;
        }

        call(_program.get_label_id(label));

        // Countering the -1 in jump
        ++_registers.pc;

        return run();
    }

    void execution_context::call(program_label_id labelId)
    {
        auto& label = _program.get_label(labelId);

        _callStack.push_back({});
        auto& frame = _callStack.back();
        frame.state = _registers;
        frame.label = labelId.idx;

        jump(label);

        if (label.stackalloc > 0)
        {
            auto tgSize = _registers.sp + label.stackalloc;
            _registers.sp = uint32_t(_stack.size());
            _stack.resize(tgSize);
        }
    }

    void execution_context::jump(program_label_id labelId)
    {
        auto& label = _program.get_label(labelId);
        jump(label);
    }

    void execution_context::jump(const program_label& label)
    {
        // Subtracting 1 because we increment pc after
        _registers.pc = label.pc - 1;
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
                case instruction::loadc:
                {
                    _registers.registers[code.reg0] =
                        _program.constants[code.arg1].value;
                    break;
                }
                case instruction::sstore:
                {
                    *reinterpret_cast<uint64_t*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].ureg;
                    break;
                }
                case instruction::sstoreu32:
                {
                    *reinterpret_cast<uint32_t*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].ureg;
                    break;
                }
                case instruction::sstoreu16:
                {
                    *reinterpret_cast<uint16_t*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].ureg;
                    break;
                }
                case instruction::sstoreu8:
                {
                    *reinterpret_cast<uint8_t*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].ureg;
                    break;
                }
                case instruction::sstorei32:
                {
                    *reinterpret_cast<int32_t*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].ireg;
                    break;
                }
                case instruction::sstorei16:
                {
                    *reinterpret_cast<int16_t*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].ireg;
                    break;
                }
                case instruction::sstorei8:
                {
                    *reinterpret_cast<int8_t*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].ireg;
                    break;
                }
                case instruction::sstoref32:
                {
                    *reinterpret_cast<float*>(
                        &_stack[_registers.registers[code.reg1].ureg]) =
                        _registers.registers[code.reg0].freg;
                    break;
                }

                case instruction::sload:
                {
                    _registers.registers[code.reg0].ureg =
                        *reinterpret_cast<uint64_t*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }
                case instruction::sloadu32:
                {
                    _registers.registers[code.reg0].ureg =
                        *reinterpret_cast<uint32_t*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }
                case instruction::sloadu16:
                {
                    _registers.registers[code.reg0].ureg =
                        *reinterpret_cast<uint16_t*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }
                case instruction::sloadu8:
                {
                    _registers.registers[code.reg0].ureg =
                        *reinterpret_cast<uint8_t*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }
                case instruction::sloadi32:
                {
                    _registers.registers[code.reg0].ireg =
                        *reinterpret_cast<int32_t*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }
                case instruction::sloadi16:
                {
                    _registers.registers[code.reg0].ireg =
                        *reinterpret_cast<int16_t*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }
                case instruction::sloadi8:
                {
                    _registers.registers[code.reg0].ireg =
                        *reinterpret_cast<int8_t*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }
                case instruction::sloadf32:
                {
                    _registers.registers[code.reg0].freg =
                        *reinterpret_cast<float*>(
                            &_stack[_registers.registers[code.reg1].ureg]);
                    break;
                }

                case instruction::utoi:
                {
                    _registers.registers[code.reg0].ireg =
                        _registers.registers[code.reg1].ureg;
                    break;
                }
                case instruction::utof:
                {
                    _registers.registers[code.reg0].freg =
                        _registers.registers[code.reg1].ureg;
                    break;
                }
                case instruction::itou:
                {
                    _registers.registers[code.reg0].ureg =
                        _registers.registers[code.reg1].ireg;
                    break;
                }
                case instruction::itof:
                {
                    _registers.registers[code.reg0].freg =
                        _registers.registers[code.reg1].ireg;
                    break;
                }
                case instruction::ftoi:
                {
                    _registers.registers[code.reg0].ireg =
                        _registers.registers[code.reg1].freg;
                    break;
                }
                case instruction::ftou:
                {
                    _registers.registers[code.reg0].ureg =
                        _registers.registers[code.reg1].freg;
                    break;
                }
                case instruction::loadi:
                    _registers.registers[code.reg0].ireg =
                        _registers.registers[code.reg1].ireg;
                    break;
                case instruction::loadu:
                    _registers.registers[code.reg0].ureg =
                        _registers.registers[code.reg1].ureg;
                    break;
                case instruction::loadf:
                    _registers.registers[code.reg0].freg =
                        _registers.registers[code.reg1].freg;
                    break;
                case instruction::addi:
                    _registers.registers[code.reg0].ireg =
                        _registers.registers[code.reg1].ireg +
                        _registers.registers[code.reg2].ireg;
                    break;
                case instruction::addu:
                    _registers.registers[code.reg0].ureg =
                        _registers.registers[code.reg1].ureg +
                        _registers.registers[code.reg2].ureg;
                    break;
                case instruction::addf:
                    _registers.registers[code.reg0].freg =
                        _registers.registers[code.reg1].freg +
                        _registers.registers[code.reg2].freg;
                    break;
                case instruction::subi:
                    _registers.registers[code.reg0].ireg =
                        _registers.registers[code.reg1].ireg -
                        _registers.registers[code.reg2].ireg;
                    break;
                case instruction::subu:
                    _registers.registers[code.reg0].ureg =
                        _registers.registers[code.reg1].ureg -
                        _registers.registers[code.reg2].ureg;
                    break;
                case instruction::subf:
                    _registers.registers[code.reg0].freg =
                        _registers.registers[code.reg1].freg -
                        _registers.registers[code.reg2].freg;
                    break;
                case instruction::muli:
                    _registers.registers[code.reg0].ireg =
                        _registers.registers[code.reg1].ireg *
                        _registers.registers[code.reg2].ireg;
                    break;
                case instruction::mulu:
                    _registers.registers[code.reg0].ureg =
                        _registers.registers[code.reg1].ureg *
                        _registers.registers[code.reg2].ureg;
                    break;
                case instruction::mulf:
                    _registers.registers[code.reg0].freg =
                        _registers.registers[code.reg1].freg *
                        _registers.registers[code.reg2].freg;
                    break;
                case instruction::divi:
                    _registers.registers[code.reg0].ireg =
                        _registers.registers[code.reg1].ireg /
                        _registers.registers[code.reg2].ireg;
                    break;
                case instruction::divu:
                    _registers.registers[code.reg0].ureg =
                        _registers.registers[code.reg1].ureg /
                        _registers.registers[code.reg2].ureg;
                    break;
                case instruction::divf:
                    _registers.registers[code.reg0].freg =
                        _registers.registers[code.reg1].freg /
                        _registers.registers[code.reg2].freg;
                    break;
                case instruction::printi:
                    printf("%zd\n", _registers.registers[code.reg0].ireg);
                    break;
                case instruction::printu:
                    printf("%zu\n", _registers.registers[code.reg0].ureg);
                    break;
                case instruction::printf:
                    printf("%f\n", _registers.registers[code.reg0].freg);
                    break;
                case instruction::prints:
                    printf("%s\n", reinterpret_cast<const char*>(
                                       _registers.registers[code.reg0].ureg));
                    break;
                case instruction::cmp:
                    _registers.cmp = _registers.registers[code.reg1].ireg -
                                     _registers.registers[code.reg0].ireg;
                    break;
                case instruction::jump:
                    // Subtracting 1 because we increment pc after
                    jump(code.warg0);
                    break;
                case instruction::jeq:
                    if (!_registers.cmp)
                    {
                        // Subtracting 1 because we increment pc after
                        jump(code.warg0);
                    }
                    break;
                case instruction::jne:
                    if (_registers.cmp)
                    {
                        // Subtracting 1 because we increment pc after
                        jump(code.warg0);
                    }
                    break;
                case instruction::call:
                {
                    call(code.warg0);
                    break;
                }
                case instruction::yield:
                {
                    _did_yield = true;
                    shouldRun = false;
                    break;
                }
                case instruction::ret:
                {
                    auto frame = _callStack.back();
                    _callStack.pop_back();
                    _registers = frame.state;

                    if (_callStack.size() == 0)
                    {
                        shouldRun = false;
                    }
                    break;
                }
                case instruction::Count:
                    break;
            }
            ++_registers.pc;
        }
        return true;
    }

}  // namespace minivm