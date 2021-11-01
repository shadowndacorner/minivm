#pragma once
#include <stdint.h>
#include <tuple>
#include <type_traits>
#include "vm.hpp"

namespace minivm
{
    template <typename T>
    static constexpr bool is_valid_external_value_type_v =
        (std::is_pointer_v<T> || std::is_integral_v<T> ||
         std::is_floating_point_v<T>)&&sizeof(T) <= sizeof(uint64_t);

    template <typename T>
    static constexpr bool is_valid_external_function_return_type_v =
        is_valid_external_value_type_v<T> || std::is_same_v<T, void>;

    struct register_manip
    {
        template <typename T>
        inline static void set_register(vm_word_t& reg, T* val)
        {
            reg.ureg = reinterpret_cast<uint64_t>(val);
        }

        inline static void set_register(vm_word_t& reg, uint64_t val)
        {
            reg.ureg = val;
        }

        inline static void set_register(vm_word_t& reg, int64_t val)
        {
            reg.ireg = val;
        }

        inline static void set_register(vm_word_t& reg, double val)
        {
            reg.freg = val;
        }

        template <typename T>
        inline static T& get(vm_word_t& reg)
        {
            static_assert(sizeof(T) == 0,
                          "Attempted to get register as invalid type");
        }

        template <typename T>
        inline static T* get_ptr(vm_word_t& reg)
        {
            return reinterpret_cast<T*>(reg.ureg);
        }
    };

    template <>
    inline uint64_t& register_manip::get<uint64_t>(vm_word_t& reg)
    {
        return reg.ureg;
    }

    template <>
    inline int64_t& register_manip::get<int64_t>(vm_word_t& reg)
    {
        return reg.ireg;
    }

    template <>
    inline double& register_manip::get<double>(vm_word_t& reg)
    {
        return reg.freg;
    }

    template <typename T>
    inline T get_register_value(vm_word_t& reg)
    {
        if constexpr (std::is_pointer_v<T>)
        {
            return register_manip::get_ptr<T>(reg);
        }
        else
        {
            return register_manip::get<T>(reg);
        }
    }

    struct program_function_utils
    {
        template <typename F, typename R, typename... Args>
        inline bool set_external_function(program& program,
                                          const std::string_view& name, F func)
        {
            return set_extern_function_internal(program, name, func, func);
        }

    private:
        template <typename F, typename R, typename... Args>
        inline bool set_extern_function_internal(program& program,
                                                 const std::string_view& name,
                                                 F func, R(Args...))
        {
            static_assert(
                is_valid_external_function_return_type_v<R>,
                "Return type must be void, pointer, or signed/unsigned "
                "integer/float type <= 8 bytes.");

            static_assert(is_valid_external_value_type_v<Args...>,
                          "Invalid argument in function provided");

            program.set_extern_function_ptr(name,
                                            &function_wrapper<F, R, Args...>);

            return true;
        }

        template <int N, typename... Ts>
        struct get_tuple_t;

        template <int N, typename T, typename... Ts>
        struct get_tuple_t<N, std::tuple<T, Ts...>>
        {
            using type = typename get_tuple_t<N - 1, std::tuple<Ts...>>::type;
        };

        template <typename T, typename... Ts>
        struct get_tuple_t<0, std::tuple<T, Ts...>>
        {
            using type = T;
        };

        template <typename Tuple, size_t... I>
        inline static Tuple create_tuple(vm_execution_registers* registers,
                                         std::index_sequence<I...>)
        {
            return tuple(get_register_value<get_tuple_t<I, Tuple>::type>(
                registers->registers[I])...);
        }

        template <typename... Args>
        inline static std::tuple<Args...> create_tuple(
            vm_execution_registers* registers)
        {
            static constexpr auto size =
                std::tuple_size<std::tuple<Args...>>::value;

            static_assert(
                size < 15,
                "Attempted to register a function with more than 15 arguments");

            return create_tuple<std::tuple<Args...>>(
                registers, std::make_index_sequence<size>());
        }

        template <typename F, typename Tuple, size_t... I>
        inline auto call_with_tuple(F f, Tuple& t, std::index_sequence<I...>)
        {
            return f(std::get<I>(t)...);
        }

        template <typename F, typename Tuple>
        inline auto call_with_tuple(F f, Tuple& t)
        {
            static constexpr auto size = std::tuple_size<Tuple>::value;
            return call_with_tuple(f, t, std::make_index_sequence<size>{});
        }

        template <typename F, typename R, typename... Args>
        inline void function_wrapper(vm_execution_registers* registers)
        {
            F f;
            auto tup = create_tuple<Args...>(registers);
            if constexpr (std::is_same_v<R, void>)
            {
                register_manip::set_register(registers[15],
                                             call_with_tuple(f, tup));
            }
            else
            {
                call_with_tuple(f, tup);
            }
        }
    };
}  // namespace minivm