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
    template <typename T, typename TT = T>
    struct cvalue_variant_visitor
    {
        TT& target;
        bool& matcher;

        inline cvalue_variant_visitor(bool& matches, TT& target)
            : target(target), matcher(matches)
        {
        }

        template <typename _>
        inline void operator()(_&)
        {
            matcher = false;
        }

        inline void operator()(T& v)
        {
            matcher = true;
            target = v;
        }
    };

    std::string& constant_value::string_ref()
    {
        return std::get<std::string>(_value);
    }

    bool constant_value::get_string(std::string_view& out)
    {
        bool match;
        cvalue_variant_visitor<std::string, std::string_view> visitor(match,
                                                                      out);
        std::visit(visitor, _value);
        return match;
    }

    bool constant_value::get_i64(int64_t& out)
    {
        bool match;
        cvalue_variant_visitor<int64_t> visitor(match, out);
        std::visit(visitor, _value);
        return match;
    }

    bool constant_value::get_u64(uint64_t& out)
    {
        bool match;
        cvalue_variant_visitor<uint64_t> visitor(match, out);
        std::visit(visitor, _value);
        return match;
    }

    bool constant_value::get_f64(double& out)
    {
        bool match;
        cvalue_variant_visitor<double> visitor(match, out);
        std::visit(visitor, _value);
        return match;
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

        bool read_opcode_register_arg(uint8_t& target)
        {
            uint16_t tg;
            bool res = read_opcode_register_arg(tg);
            target = tg;
            return res;
        }

        bool read_opcode_register_arg(uint16_t& target)
        {
            token rtok;
            if (!gettok(rtok))
            {
                error = "Expected register, got EOF";
                return false;
            }

            uint8_t reg;
            if (rtok.source[0] != 'r')
            {
                error = "Expected register, got " + std::string(rtok.source);
                return false;
            }

            if (!read_number(rtok.source.substr(1), reg))
            {
                error = "Invalid register index " + std::string(rtok.source);
                return false;
            }

            target = reg;
            return true;
        }

        bool read_opcode_constant_arg(uint32_t& target)
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
                    {"salloc", instruction::salloc},
                    {"push", instruction::push},
                    {"pop", instruction::pop},
                    {"stores", instruction::stores},
                    {"dstores", instruction::dstores},
                    {"loads", instruction::loads},
                    {"dloads", instruction::dloads},
                    {"loadic", instruction::loadic},
                    {"loaduc", instruction::loaduc},
                    {"loadfc", instruction::loadfc},
                    {"loadii", instruction::loadii},
                    {"loaduu", instruction::loaduu},
                    {"loadff", instruction::loadff},
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
                    {"printsc", instruction::printsc},
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

            switch (op.instruction)
            {
                case instruction::salloc:
                    error = "Loading salloc not yet implemented";
                    return false;
                    break;
                case instruction::push:
                    if (!read_opcode_register_arg(op.reg0)) return false;
                    break;
                case instruction::pop:
                    break;
                case instruction::dloads:
                case instruction::dstores:
                    if (!read_opcode_register_arg(op.reg0)) return false;
                    if (!read_opcode_register_arg(op.arg2)) return false;
                    break;
                case instruction::loads:
                case instruction::stores:
                    if (!read_opcode_register_arg(op.reg0)) return false;
                    if (!read_opcode_u16(op.arg2)) return false;
                    break;
                case instruction::loadic:
                case instruction::loaduc:
                case instruction::loadfc:
                {
                    if (!read_opcode_register_arg(op.arg2)) return false;
                    if (!read_opcode_constant_arg(op.warg0)) return false;
                    break;
                }
                case instruction::cmp:
                case instruction::loadii:
                case instruction::loaduu:
                case instruction::loadff:
                {
                    if (!read_opcode_register_arg(op.arg0)) return false;
                    if (!read_opcode_register_arg(op.arg1)) return false;
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
                    if (!read_opcode_register_arg(op.arg0)) return false;
                    if (!read_opcode_register_arg(op.arg1)) return false;
                    break;
                }
                case instruction::printi:
                case instruction::printu:
                case instruction::printf:
                {
                    if (!read_opcode_register_arg(op.arg0)) return false;
                    break;
                }
                case instruction::printsc:
                {
                    if (!read_opcode_constant_arg(op.warg0)) return false;
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
                        op.arg2 = 0;
                    }
                    else
                    {
                        op.warg0 = 0;

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

                        op.arg2 = 1;
                    }

                    break;
                }
                default:
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
                val.move(std::move(str));
                success = true;
            }
            // TODO: Clean the following up
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
                        if (op.arg2)
                        {
                            auto& label = future_labels[op.warg0];
                            if (!program.labels.count(label))
                            {
                                error = "Jump to unknown label " + label;
                                return false;
                            }

                            op.warg0 = program.labels[label];
                        }
                        break;
                    default:
                        break;
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
            return postprocess_labels();
        }

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