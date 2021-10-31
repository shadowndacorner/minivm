#include <stdint.h>
#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include <minivm/vm.hpp>

namespace minivm
{
    constant_value::constant_value()
        : value({0}), is_data_offset(false), is_pointer(false)
    {
    }

    static bool is_whitespace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static bool is_unsigned_start(char c)
    {
        return c == 'u';
    }

    static bool is_signed_start(char c)
    {
        return c == 's';
    }

    static bool is_float_start(char c)
    {
        return c == 'f';
    }

    static bool is_string_terminal(char c)
    {
        return c == '"';
    }

    static bool is_constant_start(char c)
    {
        return c == '$';
    }

    static bool is_label_start(char c)
    {
        return c == '.';
    }

    static bool is_comment_start(char c)
    {
        return c == '#';
    }

    static bool is_numeric(char c)
    {
        return c >= '0' && c <= '9';
    }

    struct asm_parser
    {
        asm_parser(program& prog, const std::string_view& source)
            : source(source), program(prog), offset(0)
        {
        }

        char peekchar()
        {
            if (offset >= source.size()) return 0;
            return source[offset];
        }

        char getchar()
        {
            if (offset >= source.size()) return 0;
            return source[offset++];
        }

        void skip_whitespace()
        {
            char peek = peekchar();
            while (is_whitespace(peek))
            {
                getchar();
                peek = peekchar();
            }
        }

        struct token
        {
            enum class toktype
            {
                label,
                ident,
                cname,
            } type;

            std::string_view source;
        };

        bool handle_escape_sequence(std::string& out)
        {
            char c = getchar();
            if (c == 'x')
            {
                uint32_t start = offset;
                while ((c = peekchar()) && is_numeric(c))
                {
                    getchar();
                }

                std::string_view str = source.substr(start, offset - start);
                auto res =
                    std::from_chars(str.data(), str.data() + str.size(), c);
                if (res.ec == std::errc())
                {
                    out += c;
                    return true;
                }
                else
                {
                    switch (res.ec)
                    {
                        case std::errc::invalid_argument:
                            error = "Escape sequence [\\x" + std::string(str) +
                                    "] is not a valid number";
                            break;
                        case std::errc::result_out_of_range:
                            error = "Escape sequence [\\x" + std::string(str) +
                                    "] is larger than would fit in a char";
                            break;
                        default:
                            error = "Escape sequence [\\x" + std::string(str) +
                                    "] invalid with unknown error";
                            break;
                    }
                }
            }
            else if (c == 'n')
            {
                out += '\n';
                return true;
            }

            out += c;
            return true;
        }

        bool read_constant_string(std::string& out)
        {
            out.clear();

            // Reserve 64 chars to start with
            out.reserve(64);

            char c = getchar();
            if (!c)
            {
                error = "Reached EOF";
                return false;
            }

            uint32_t start;
            if (is_string_terminal(c))
            {
                start = offset;
            }
            else
            {
                error =
                    "Attempted to read constant string that did not begin with "
                    "string terminal";
                return false;
            }

            while ((c = getchar()))
            {
                if (c == '\\')
                {
                    if (!handle_escape_sequence(out))
                    {
                        return false;
                    }
                    continue;
                }

                if (is_string_terminal(c))
                {
                    return true;
                }

                out += c;
            }
            error = "Reached EOF";
            return false;
        }

        std::string_view read_ident_source(int32_t toff)
        {
            char c;
            uint32_t start = offset + toff;
            while ((c = getchar()))
            {
                if (is_whitespace(c))
                {
                    return source.substr(start, offset - start - 1);
                }
            }
            return source.substr(start, offset - start);
        }

        bool is_eof()
        {
            return offset >= source.size();
        }

        bool gettok(token& tok)
        {
            char c;
            while ((c = getchar()))
            {
                if (is_comment_start(c))
                {
                    // Read until newline or EOF if we get a comment
                    while ((c = getchar()) && c != '\n')
                    {
                    }
                    continue;
                }

                if (is_whitespace(c)) continue;
                if (is_constant_start(c))
                {
                    tok.type = token::toktype::cname;
                    tok.source = read_ident_source(0);
                    return true;
                }

                if (is_label_start(c))
                {
                    tok.type = token::toktype::label;
                    tok.source = read_ident_source(0);
                    return true;
                }

                tok.type = token::toktype::ident;
                tok.source = read_ident_source(-1);
                return true;
            }
            return false;
        }

        bool read_label(token& label)
        {
            std::string str(label.source);
            if (program.labels.count(str))
            {
                error = "Duplicate label " + str + " detected";
                return false;
            }
            program.labels.insert({str, uint32_t(program.opcodes.size())});
            return true;
        }

        uint8_t read_opcode_register_arg(bool& success)
        {
            token rtok;
            if (!gettok(rtok))
            {
                error = "Expected register, got EOF";
                success = false;
                return 0;
            }

            uint8_t reg;
            if (rtok.source[0] != 'r')
            {
                error = "Expected register, got " + std::string(rtok.source);
                success = false;
                return 0;
            }

            if (!read_number(rtok.source.substr(1), reg))
            {
                error = "Invalid register index " + std::string(rtok.source);
                success = false;
                return 0;
            }

            success = true;
            return reg;
        }

        bool read_opcode_constant_arg(uint16_t& target)
        {
            token ctok;
            if (!gettok(ctok))
            {
                error = "Expected constant, got EOF";
                return false;
            }

            std::string_view constant = ctok.source;

            std::string ctokSrc(ctok.source);
            if (!constantMap.count(ctokSrc))
            {
                error =
                    "Instruction attempted to use unknown "
                    "constant [" +
                    ctokSrc + "]";
                return false;
            }
            target = constantMap[ctokSrc];
            return true;
        }

        bool read_opcode_u16(uint16_t& target)
        {
            token ctok;
            if (!gettok(ctok))
            {
                error = "Expected constant, got EOF";
                return false;
            }

            if (!read_number(ctok.source, target))
            {
                error = "Expected number, got " + std::string(ctok.source);
                return false;
            }
            return true;
        }

        bool read_opcode(token& instruction)
        {
            // regexr generator
            /*
            ([A-Za-z]+),
            { "$1", instruction::$1 },\n
            */
            static std::unordered_map<std::string_view, minivm::instruction>
                map = {
                    // Generated
                    {"loadc", instruction::loadc},
                    {"stores", instruction::stores},
                    {"loads", instruction::loads},
                    {"loadi", instruction::loadi},
                    {"loadu", instruction::loadu},
                    {"loadf", instruction::loadf},
                    {"utoi", instruction::utoi},
                    {"utof", instruction::utof},
                    {"itou", instruction::itou},
                    {"itof", instruction::itof},
                    {"ftoi", instruction::ftoi},
                    {"ftou", instruction::ftou},
                    {"addi", instruction::addi},
                    {"addu", instruction::addu},
                    {"addf", instruction::addf},
                    {"subi", instruction::subi},
                    {"subu", instruction::subu},
                    {"subf", instruction::subf},
                    {"muli", instruction::muli},
                    {"mulu", instruction::mulu},
                    {"mulf", instruction::mulf},
                    {"divi", instruction::divi},
                    {"divu", instruction::divu},
                    {"divf", instruction::divf},
                    {"printi", instruction::printi},
                    {"printu", instruction::printu},
                    {"printf", instruction::printf},
                    {"prints", instruction::prints},
                    {"yield", instruction::yield},
                    {"cmp", instruction::cmp},
                    {"jump", instruction::jump},
                    {"jeq", instruction::jeq},
                    {"jne", instruction::jne},
                    // End generated
                };

            opcode op;
            if (map.count(instruction.source))
            {
                op.instruction = map[instruction.source];
            }
            else
            {
                error =
                    "Unknown instruction " + std::string(instruction.source);
                return false;
            }

            bool success = true;
            switch (op.instruction)
            {
                case instruction::loadc:
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;

                    if (!read_opcode_constant_arg(op.sarg1)) return false;
                    break;
                case instruction::loads:
                case instruction::stores:
                {
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;

                    op.reg1 = read_opcode_register_arg(success);
                    if (!success) return false;
                    break;
                }
                case instruction::cmp:
                case instruction::loadi:
                case instruction::loadu:
                case instruction::loadf:
                {
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;

                    op.reg1 = read_opcode_register_arg(success);
                    if (!success) return false;

                    break;
                }

                case instruction::utoi:
                case instruction::utof:
                case instruction::itou:
                case instruction::itof:
                case instruction::ftoi:
                case instruction::ftou:
                {
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;

                    op.reg1 = read_opcode_register_arg(success);
                    if (!success) return false;

                    break;
                }

                case instruction::addi:
                case instruction::addu:
                case instruction::addf:
                case instruction::subi:
                case instruction::subu:
                case instruction::subf:
                case instruction::muli:
                case instruction::mulu:
                case instruction::mulf:
                case instruction::divi:
                case instruction::divu:
                case instruction::divf:
                {
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;

                    op.reg1 = read_opcode_register_arg(success);
                    if (!success) return false;

                    op.reg2 = read_opcode_register_arg(success);
                    if (!success) return false;

                    break;
                }

                case instruction::printi:
                case instruction::printu:
                case instruction::printf:
                case instruction::prints:
                {
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;
                    break;
                }

                case instruction::yield:
                    // No arguments
                    break;
                case instruction::jump:
                case instruction::jne:
                case instruction::jeq:
                {
                    token labelTok;
                    if (!gettok(labelTok))
                    {
                        error = "Failed to read argument for " +
                                std::string(instruction.source) + ": EOF";
                    }

                    if (labelTok.type != token::toktype::label)
                    {
                        error = "Expected label for " +
                                std::string(instruction.source) + ", got " +
                                std::string(labelTok.source);

                        return false;
                    }

                    std::string label(labelTok.source);
                    if (program.labels.count(label))
                    {
                        op.warg0 = program.labels[label];
                        op.sarg1 = 0;
                    }
                    else
                    {
                        op.warg0 = 0;
                        op.sarg1 = 1;

                        for (size_t i = 0; i < future_labels.size(); ++i)
                        {
                            if (future_labels[i] == label)
                            {
                                op.warg0 = i + 1;
                            }
                        }

                        if (op.warg0 == 0)
                        {
                            op.warg0 = uint32_t(future_labels.size());
                            future_labels.push_back(label);
                        }
                        else
                        {
                            --op.warg0;
                        }
                    }

                    break;
                }
                case instruction::Count:
                {
                    error = "Loader for instruction " +
                            std::string(instruction.source) +
                            " not implemented";
                    return false;
                }
            }

            program.opcodes.push_back(op);
            return true;
        }

        template <typename T>
        inline bool read_number(const std::string_view& str, T& result)
        {
            auto res =
                std::from_chars(str.data(), str.data() + str.size(), result);

            if (res.ec == std::errc())
            {
                return true;
            }
            else
            {
                switch (res.ec)
                {
                    case std::errc::invalid_argument:
                        error = "Escape sequence [\\x" + std::string(str) +
                                "] is not a valid number";
                        break;
                    case std::errc::result_out_of_range:
                        error = "Escape sequence [\\x" + std::string(str) +
                                "] is larger than would fit in a char";
                        break;
                    default:
                        error = "Escape sequence [\\x" + std::string(str) +
                                "] invalid with unknown error";
                        break;
                }
            }
            return false;
        }

        template <typename T>
        inline bool read_numeric_constant_value(constant_value& val)
        {
            getchar();
            T result;
            uint32_t start = offset;

            char peeked;
            while ((peeked = peekchar()) && !is_whitespace(peeked))
            {
                getchar();
            }

            auto str = source.substr(start, offset - start);
            auto res =
                std::from_chars(str.data(), str.data() + str.size(), result);

            if (res.ec == std::errc())
            {
                val.set(result);
                return true;
            }
            else
            {
                switch (res.ec)
                {
                    case std::errc::invalid_argument:
                        error =
                            "[" + std::string(str) + "] is not a valid number";
                        break;
                    case std::errc::result_out_of_range:
                        error = "[" + std::string(str) +
                                "] is larger than would fit in a char";
                        break;
                    default:
                        error = "[" + std::string(str) +
                                "] invalid with unknown error";
                        break;
                }
            }
            return false;
        }

        bool read_constant(token& nameTok)
        {
            std::string name = std::string(nameTok.source);

            if (constantMap.count(name))
            {
                error = "Constant redefinition: [" + name + "] already exists";
                return false;
            }

            skip_whitespace();

            constant_value val;

            char peeked = peekchar();
            if (!peeked)
            {
                error = "Failed to read constant [" + name + "]: EOF";
                return false;
            }

            bool success = false;

            if (is_string_terminal(peeked))
            {
                std::string str;
                if (!read_constant_string(str))
                {
                    error = "Failed to read constant [" + name + "]: " + error;

                    return false;
                }

                // If it's in the string table already, then don't duplicate it.
                if (constantStringTable.count(str))
                {
                    val.value.ureg = constantStringTable[str];
                }
                else
                {
                    // Copy if it isn't in the string table
                    size_t pos = program._data.size();
                    size_t start = pos;
                    program._data.resize(program._data.size() + str.size() + 1);

                    // Copy string content
                    for (size_t i = 0; i < str.size(); ++i)
                    {
                        program._data[pos++] = str[i];
                    }

                    // Null terminate
                    program._data[pos] = 0;

                    val.value.ureg = start;
                    constantStringTable[str] = start;
                }
                val.is_data_offset = true;
                success = true;
            }
            else if (is_unsigned_start(peeked))
            {
                success = read_numeric_constant_value<uint64_t>(val);
            }
            else if (is_signed_start(peeked))
            {
                success = read_numeric_constant_value<int64_t>(val);
            }
            else if (is_float_start(peeked))
            {
                success = read_numeric_constant_value<double>(val);
            }

            if (!success)
            {
                error = "Failed to read constant [" + name + "]: " +
                        (error.size() == 0 ? "Value had unknown type" : error);

                return false;
            }

            constantMap.insert({name, uint32_t(program.constants.size())});
            program.constants.push_back(val);
            return true;
        }

        bool postprocess_labels()
        {
            if (future_labels.size() == 0) return true;
            for (auto& op : program.opcodes)
            {
                switch (op.instruction)
                {
                    case instruction::jump:
                    case instruction::jne:
                    case instruction::jeq:
                        if (op.sarg1)
                        {
                            auto& label = future_labels[op.warg0];
                            if (!program.labels.count(label))
                            {
                                error = "Jump to unknown label " + label;
                                return false;
                            }
                            op.warg0 = program.labels[label];
                            op.sarg1 = 0;
                        }
                        break;
                    default:
                        break;
                }
            }
            return true;
        }

        bool postprocess_constant_values()
        {
            for (auto& cval : program.constants)
            {
                if (cval.is_data_offset)
                {
                    cval.value.ureg = reinterpret_cast<uint64_t>(
                        &program._data[cval.value.ureg]);

                    cval.is_data_offset = false;
                    cval.is_pointer = true;
                }
            }
            return true;
        }

        bool parse()
        {
            token tok;
            while (gettok(tok))
            {
                switch (tok.type)
                {
                    case token::toktype::label:
                        if (!read_label(tok)) return false;
                        break;
                    case token::toktype::ident:
                        if (!read_opcode(tok)) return false;
                        break;
                    case token::toktype::cname:
                        if (!read_constant(tok)) return false;
                        break;
                    default:
                        error = "Unknown token " + std::string(tok.source);
                        return false;
                }
            }
            return postprocess_labels() && postprocess_constant_values();
        }

        std::unordered_map<std::string, uint64_t> constantStringTable;
        std::unordered_map<std::string, uint32_t> constantMap;
        std::string error;
        std::string_view source;
        uint64_t offset;

        std::vector<std::string> future_labels;
        std::string_view cur_label;
        program& program;
    };

    bool program::load_assembly(const std::string_view& mvmaSrc)
    {
        asm_parser parser(*this, mvmaSrc);
        if (!parser.parse())
        {
            load_error = parser.error;
            return false;
        }
        return true;
    }

    bool program::load_assembly_from_file(const std::string_view& filename)
    {
        printf("Loading from file %s\n", filename.data());

        std::ifstream stream(filename.data(), std::ios_base::binary);

        if (!stream.good())
        {
            load_error = "Failed to open file " + std::string(filename);
            return false;
        }

        stream.seekg(0, std::ios::end);

        size_t size = stream.tellg();
        std::string buffer(size, ' ');
        stream.seekg(0);

        stream.read(buffer.data(), size);
        return load_assembly(buffer);
    }

    const char* program::get_load_error()
    {
        return load_error.c_str();
    }

}  // namespace minivm