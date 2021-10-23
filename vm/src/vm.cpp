#include <eh.h>
#include <stdint.h>
#include <fstream>
#include <minivm/vm.hpp>
#include <string_view>
#include <unordered_map>
#include <variant>

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

    bool constant_value::as_f64(double& out)
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

    static bool is_string_start(char c)
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

    struct asm_parser
    {
        asm_parser(program& prog, const std::string_view& source)
            : source(source), program(prog), cur_opcode(0), offset(0)
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

        struct token
        {
            enum class toktype
            {
                label,
                ident,
                cname,
            } type;

            std::string_view source;
            int64_t cintval;
        };

        std::string_view read_constant_string() {}

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

                // TODO: Detect constants
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
            program.labels.insert({str, 0});
            return true;
        }

        bool read_opcode(token& instruction)
        {
            // regexr
            /*
            ([A-Za-z]+)
            { "$1", instruction::$1 },\n
            */
            static std::unordered_map<std::string, minivm::instruction> map = {
                {"load", instruction::load},
                {"add", instruction::add},
                {"printi", instruction::printi},
                {"jump", instruction::jump}};

            error = "Unknown instruction " + std::string(instruction.source);
            return false;
        }

        bool read_constant(token& name)
        {
            token value;
            if (!gettok(value))
            {
                error = "Failed to read constant [" + std::string(name.source) +
                        "]: " + (is_eof() ? "Reached EOF" : error);
                return false;
            }

            error = "Failed to read constant [" + std::string(name.source) +
                    "]: Value had unknown type";
            return false;
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
            return true;
        }

        std::string error;
        std::string_view source;
        uint64_t offset;

        uint32_t cur_opcode;
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