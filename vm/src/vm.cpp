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
        return c == 'i';
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

    // Externs can be either labels or functions
    static bool is_extern_start(char c)
    {
        return c == '@';
    }

    static bool is_label_start(char c)
    {
        return c == '.';
    }

    static bool is_comment_start(char c)
    {
        return c == '#' || c == ';';
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
                external,
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

        bool read_string_literal(std::string& out)
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
                else if (is_extern_start(c))
                {
                    tok.type = token::toktype::external;
                    tok.source = read_ident_source(0);
                    return true;
                }
                else if (is_label_start(c))
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

        bool read_external(token& label)
        {
            char start = label.source[0];

            std::string name(label.source);
            if (program.extern_map.count(name))
            {
                error = "Duplicate external " + name;
                return false;
            }

            auto idx = program.externs.size();
            program.externs.push_back({0});
            program.extern_map[name] = idx;
            return true;
        }

        bool read_label(token& label)
        {
            std::string str(label.source);
            if (program.label_map.count(str))
            {
                error = "Duplicate label " + str + " detected";
                return false;
            }

            program_label newLabel;
            newLabel.offset = program.write_static_string(label.source);
            newLabel.pc = program.opcodes.size();
            newLabel.stackalloc = 0;

            // Check to see if a number is next
            skip_whitespace();
            if (is_numeric(peekchar()))
            {
                token numtok;
                if (!gettok(numtok))
                {
                    error =
                        "Unexpected EOF while reading stackalloc for label " +
                        str;
                    return false;
                }

                if (!read_number(numtok.source, newLabel.stackalloc))
                {
                    error = "Failed to read stackalloc for label " + str +
                            " - " + error;
                    return false;
                }
            }

            program.labels.push_back(newLabel);
            program.label_map.insert(
                {str, uint32_t(program.labels.size() - 1)});

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
            // Kind of hacky, but we can use this to read the constant value
            // without code duplication
            auto ogOffset = offset;

            token ctok;
            if (!gettok(ctok))
            {
                error = "Expected constant, got EOF";
                return false;
            }

            std::string_view constant = ctok.source;
            std::string ctokSrc(ctok.source);
            if (ctok.type == token::toktype::cname)
            {
                if (!constantMap.count(ctokSrc))
                {
                    error =
                        "Instruction attempted to use unknown "
                        "constant [" +
                        ctokSrc + "]";
                    return false;
                }
                target = constantMap[ctokSrc];
            }
            else if (is_signed_start(constant[0]) ||
                     is_unsigned_start(constant[0]) ||
                     is_float_start(constant[0]) ||
                     is_string_terminal(constant[0]) ||
                     is_unsigned_start(constant[0]))
            {
                offset = ogOffset;

                std::string key;
                if (is_string_terminal(constant[0]))
                {
                    auto prevOffset = offset;
                    if (!read_string_literal(key))
                    {
                        error = "Failed to read inline constant - " + error;
                        return false;
                    }
                    offset = prevOffset;
                }
                else
                {
                    key = ctok.source;
                }

                key = "%_impl_" + key;

                token nametok;
                nametok.source = key;
                if (!read_constant(nametok, true))
                {
                    error = "Failed to read inline constant - " + error;
                    return false;
                }

                target = constantMap[key];
            }
            else
            {
                error = "Expected constant name, string, or number - got " +
                        std::string(ctok.source);
                return false;
            }
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

        bool read_opcode_label(uint32_t& target)
        {
            token labelTok;
            if (!gettok(labelTok))
            {
                error = "EOF";
            }

            if (labelTok.type != token::toktype::label)
            {
                error = "Expected label, got " + std::string(labelTok.source);
                return false;
            }

            std::string label(labelTok.source);
            if (program.label_map.count(label))
            {
                target = program.get_label_id(label).idx;
            }
            else
            {
                target = 0;

                for (size_t i = 0; i < future_labels.size(); ++i)
                {
                    if (future_labels[i] == label)
                    {
                        target = i + 1;
                        break;
                    }
                }

                if (target == 0)
                {
                    target = uint32_t(future_labels.size());
                    future_labels.push_back(label);
                }
                else
                {
                    --target;
                }
                target |= (1 << 31);
            }
            return true;
        }

        bool read_opcode_external(uint32_t& target)
        {
            token labelTok;
            if (!gettok(labelTok))
            {
                error = "EOF";
            }

            if (labelTok.type != token::toktype::external)
            {
                error =
                    "Expected external, got " + std::string(labelTok.source);
                return false;
            }

            std::string external(labelTok.source);
            if (program.extern_map.count(external))
            {
                target = program.get_extern_id(external).idx;
            }
            else
            {
                error = "Failed to locate external " + external;
                return false;
            }
            return true;
        }

        bool read_opcode_external(uint16_t& target)
        {
            token labelTok;
            if (!gettok(labelTok))
            {
                error = "EOF";
            }

            if (labelTok.type != token::toktype::external)
            {
                error =
                    "Expected external, got " + std::string(labelTok.source);
                return false;
            }

            std::string external(labelTok.source);
            if (program.extern_map.count(external))
            {
                target = program.get_extern_id(external).idx;
            }
            else
            {
                error = "Failed to locate external " + external;
                return false;
            }
            return true;
        }

        template <typename T>
        bool read_opcode_number_value(T& target)
        {
            token labelTok;
            if (!gettok(labelTok))
            {
                error = "EOF";
            }

            if (labelTok.type != token::toktype::label)
            {
                error = "Expected number, got " + std::string(labelTok.source);
                return false;
            }

            if (!read_number(labelTok.source, target)) return false;
            return true;
        }

        bool read_opcode(token& instruction)
        {
            // regexr generator
            /*
            ([A-Za-z0-9]+),
            { "$1", instruction::$1 },\n
            */
            static std::unordered_map<std::string_view, minivm::instruction>
                map = {
                    // Generated
                    {"loadc", instruction::loadc},
                    {"eload", instruction::eload},
                    {"estore", instruction::estore},
                    {"sstore", instruction::sstore},
                    {"sstoreu32", instruction::sstoreu32},
                    {"sstoreu16", instruction::sstoreu16},
                    {"sstoreu8", instruction::sstoreu8},
                    {"sstorei32", instruction::sstorei32},
                    {"sstorei16", instruction::sstorei16},
                    {"sstorei8", instruction::sstorei8},
                    {"sstoref32", instruction::sstoref32},
                    {"sload", instruction::sload},
                    {"sloadu32", instruction::sloadu32},
                    {"sloadu16", instruction::sloadu16},
                    {"sloadu8", instruction::sloadu8},
                    {"sloadi32", instruction::sloadi32},
                    {"sloadi16", instruction::sloadi16},
                    {"sloadi8", instruction::sloadi8},
                    {"sloadf32", instruction::sloadf32},
                    {"mov", instruction::mov},
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
                    {"cmp", instruction::cmp},
                    {"jump", instruction::jump},
                    {"jeq", instruction::jeq},
                    {"jne", instruction::jne},
                    {"call", instruction::call},
                    {"callext", instruction::callext},
                    {"yield", instruction::yield},
                    {"ret", instruction::ret},
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

                    if (!read_opcode_constant_arg(op.arg1)) return false;
                    break;
                case instruction::estore:
                case instruction::eload:
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;

                    if (!read_opcode_external(op.arg1)) return false;
                    break;
                case instruction::sstore:
                case instruction::sstoreu32:
                case instruction::sstoreu16:
                case instruction::sstoreu8:
                case instruction::sstorei32:
                case instruction::sstorei16:
                case instruction::sstorei8:
                case instruction::sstoref32:
                case instruction::sload:
                case instruction::sloadu32:
                case instruction::sloadu16:
                case instruction::sloadu8:
                case instruction::sloadi32:
                case instruction::sloadi16:
                case instruction::sloadi8:
                case instruction::sloadf32:
                {
                    op.reg0 = read_opcode_register_arg(success);
                    if (!success) return false;

                    if (!read_opcode_number_value(op.arg1)) return false;
                    break;
                }
                case instruction::cmp:
                case instruction::mov:
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

                case instruction::jump:
                case instruction::jne:
                case instruction::jeq:
                {
                    if (!read_opcode_label(op.warg0)) return false;
                    break;
                }
                case instruction::call:
                    if (!read_opcode_label(op.warg0)) return false;
                    break;
                case instruction::callext:
                    if (!read_opcode_external(op.warg0)) return false;
                    break;
                case instruction::yield:
                case instruction::ret:
                    // No arguments
                    break;
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

        inline bool read_string_into_constant_value(constant_value& val)
        {
            std::string str;
            if (!read_string_literal(str))
            {
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
                auto start = program.write_static_string(str);
                val.value.ureg = start;
                constantStringTable[str] = start;
            }
            val.is_data_offset = true;
            return true;
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

        bool read_constant(constant_value& val)
        {
            char peeked = peekchar();
            if (!peeked)
            {
                error = "EOF";
                return false;
            }

            bool success = false;

            if (is_string_terminal(peeked))
            {
                success = read_string_into_constant_value(val);
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
                error = (error.size() == 0 ? "Value had unknown type" : error);

                return false;
            }
            return true;
        }

        bool read_constant(token& nameTok, constant_value& val,
                           bool ignoreDuplicates)
        {
            bool hasDuplicate = false;
            std::string name = std::string(nameTok.source);
            if (constantMap.count(name))
            {
                hasDuplicate = true;
                if (!ignoreDuplicates)
                {
                    error =
                        "Constant redefinition: [" + name + "] already exists";
                    return false;
                }
            }

            skip_whitespace();

            if (!read_constant(val))
            {
                error = "Failed to read constant [" + name + "]: " + error;
            }

            if (!hasDuplicate)
            {
                constantMap.insert({name, uint32_t(program.constants.size())});
                program.constants.push_back(val);
            }
            return true;
        }

        bool read_constant(token& nameTok, bool ignoreDuplicates = false)
        {
            constant_value val;
            return read_constant(nameTok, val, ignoreDuplicates);
        }

        bool postprocess_labels()
        {
            for (auto& label : program.labels)
            {
                label.name = &program._data[label.offset];
            }
            return true;
        }

        bool postprocess_label_references()
        {
            if (future_labels.size() == 0) return true;
            for (auto& op : program.opcodes)
            {
                switch (op.instruction)
                {
                    case instruction::call:
                    case instruction::jump:
                    case instruction::jne:
                    case instruction::jeq:
                        if (op.warg0 & (1 << 31))
                        {
                            op.warg0 &= ~(1 << 31);

                            auto& label = future_labels[op.warg0];
                            if (!program.label_map.count(label))
                            {
                                error = "Jump to unknown label " + label;
                                return false;
                            }
                            op.warg0 = program.get_label_id(label).idx;
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
                    case token::toktype::external:
                        if (!read_external(tok)) return false;
                        break;
                    case token::toktype::label:
                        if (!read_label(tok)) return false;
                        break;
                    case token::toktype::ident:
                        if (!read_opcode(tok)) return false;
                        break;
                    case token::toktype::cname:
                        if (!read_constant(tok)) return false;
                        break;
                }
            }
            return postprocess_labels() && postprocess_label_references() &&
                   postprocess_constant_values();
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

    bool program::set_extern_function_ptr(const std::string_view& name,
                                          extern_program_func_t func)
    {
        return set_extern_pointer(name, func);
    }

    bool program::set_unsigned_extern(const std::string_view& name,
                                      uint64_t value)
    {
        auto namev = std::string(name);
        if (extern_map.count(namev))
        {
            get_extern(extern_map[namev]).value.ureg = value;
            return true;
        }
        return false;
    }

    bool program::set_signed_extern(const std::string_view& name, int64_t value)
    {
        auto namev = std::string(name);
        if (extern_map.count(namev))
        {
            get_extern(extern_map[namev]).value.ireg = value;
            return true;
        }
        return false;
    }

    bool program::set_floating_extern(const std::string_view& name,
                                      double value)
    {
        auto namev = std::string(name);
        if (extern_map.count(namev))
        {
            get_extern(extern_map[namev]).value.freg = value;
            return true;
        }
        return false;
    }

    bool program::get_extern_ptr(const std::string_view& name, uint64_t** value)
    {
        auto namev = std::string(name);
        if (extern_map.count(namev))
        {
            *value = &get_extern(extern_map[namev]).value.ureg;
            return true;
        }
        *value = 0;
        return false;
    }

    bool program::get_extern_ptr(const std::string_view& name, int64_t** value)
    {
        auto namev = std::string(name);
        if (extern_map.count(namev))
        {
            *value = &get_extern(extern_map[namev]).value.ireg;
            return true;
        }
        *value = 0;
        return false;
    }

    bool program::get_extern_ptr(const std::string_view& name, double** value)
    {
        auto namev = std::string(name);
        if (extern_map.count(namev))
        {
            *value = &get_extern(extern_map[namev]).value.freg;
            return true;
        }
        *value = 0;
        return false;
    }

    uint32_t program::write_static_string(const std::string_view& str)
    {
        uint32_t pos = _data.size();
        uint32_t start = pos;
        _data.resize(_data.size() + str.size() + 1);

        // Copy string content
        for (uint32_t i = 0; i < str.size(); ++i)
        {
            _data[pos++] = str[i];
        }

        // Null terminate
        _data[pos] = 0;
        return start;
    }

    program_label_id_t program::get_label_id(const std::string_view& label)
    {
        // TODO: This doesn't need to allocate
        return label_map[std::string(label)];
    }

    program_label& program::get_label(const std::string_view& label)
    {
        return get_label(get_label_id(label));
    }

    program_label& program::get_label(program_label_id_t id)
    {
        return labels[id.idx];
    }

    program_extern_id_t program::get_extern_id(const std::string_view& label)
    {
        // TODO: This doesn't need to allocate
        return extern_map[std::string(label)];
    }

    program_extern_value& program::get_extern(const std::string_view& label)
    {
        return get_extern(get_extern_id(label));
    }

    program_extern_value& program::get_extern(program_extern_id_t id)
    {
        return externs[id.idx];
    }

}  // namespace minivm