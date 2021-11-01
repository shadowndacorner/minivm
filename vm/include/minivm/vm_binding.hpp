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

        inline static void set_register(vm_word_t& reg, uint32_t val)
        {
            reg.ureg = val;
        }

        inline static void set_register(vm_word_t& reg, int32_t val)
        {
            reg.ireg = val;
        }

        inline static void set_register(vm_word_t& reg, uint16_t val)
        {
            reg.ureg = val;
        }

        inline static void set_register(vm_word_t& reg, int16_t val)
        {
            reg.ireg = val;
        }

        inline static void set_register(vm_word_t& reg, uint8_t val)
        {
            reg.ureg = val;
        }

        inline static void set_register(vm_word_t& reg, int8_t val)
        {
            reg.ireg = val;
        }

        inline static void set_register(vm_word_t& reg, double val)
        {
            reg.freg = val;
        }

        inline static void set_register(vm_word_t& reg, float val)
        {
            reg.freg = val;
        }

        template <typename T>
        inline static T get(vm_word_t& reg)
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
    inline uint64_t register_manip::get<uint64_t>(vm_word_t& reg)
    {
        return reg.ureg;
    }

    template <>
    inline int64_t register_manip::get<int64_t>(vm_word_t& reg)
    {
        return reg.ireg;
    }

    template <>
    inline double register_manip::get<double>(vm_word_t& reg)
    {
        return reg.freg;
    }

    template <>
    inline uint32_t register_manip::get<uint32_t>(vm_word_t& reg)
    {
        return reg.ureg;
    }

    template <>
    inline int32_t register_manip::get<int32_t>(vm_word_t& reg)
    {
        return reg.ireg;
    }

    template <>
    inline uint16_t register_manip::get<uint16_t>(vm_word_t& reg)
    {
        return reg.ureg;
    }

    template <>
    inline int16_t register_manip::get<int16_t>(vm_word_t& reg)
    {
        return reg.ireg;
    }

    template <>
    inline uint8_t register_manip::get<uint8_t>(vm_word_t& reg)
    {
        return reg.ureg;
    }

    template <>
    inline int8_t register_manip::get<int8_t>(vm_word_t& reg)
    {
        return reg.ireg;
    }

    template <>
    inline float register_manip::get<float>(vm_word_t& reg)
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

    struct program_binding
    {
        template <auto ptr>
        inline static bool set_external_function(program& program,
                                                 const std::string_view& name)
        {
            return set_extern_function_internal<ptr>(program, name, ptr);
        }

    private:
        template <typename R, typename... Args>
        struct ctime_fn_ptr_generator
        {
            typedef R (*fn_ptr_t)(Args...);

            template <fn_ptr_t ptr>
            inline constexpr static fn_ptr_t generate()
            {
                return ptr;
            }
        };

    public:
        template <typename R, typename... Args>
        struct wrapper_fn_generator
        {
            typedef R (*fn_ptr_t)(Args...);

            template <fn_ptr_t ptr>
            inline static void call(minivm::vm_execution_registers* registers)
            {
                auto tup = create_tuple<Args...>(registers);
                if constexpr (std::is_same_v<R, void>)
                {
                    call_with_tuple(ptr, tup);
                    registers->registers[0].ureg = 0;
                }
                else
                {
                    register_manip::set_register(registers->registers[0],
                                                 call_with_tuple(ptr, tup));
                }
            }
        };

    private:
        template <typename Arg>
        inline static void check_params()
        {
            static_assert(is_valid_external_value_type_v<Arg>,
                          "Invalid argument in function provided");
        }

        template <typename Arg1, typename Arg2, typename... Args>
        inline static void check_params()
        {
            program_binding::check_params<Arg1>();
            program_binding::check_params<Arg2, Args...>();
        }

        template <auto ptr, typename R, typename... Args>
        inline static bool set_extern_function_internal(
            program& program, const std::string_view& name, R(Args...))
        {
            if constexpr (!std::is_void_v<R>)
            {
                static_assert(
                    is_valid_external_value_type_v<R>,
                    "Return type must be void, pointer, or signed/unsigned "
                    "integer/float type <= 8 bytes.");
            }

            if constexpr (sizeof...(Args) > 0)
            {
                program_binding::check_params<Args...>();
            }

            program.set_extern_function_ptr(
                name, &wrapper_fn_generator<R, Args...>::template call<ptr>);

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
            return Tuple(
                get_register_value<typename get_tuple_t<I, Tuple>::type>(
                    registers->registers[I])...);
        }

        template <typename... Args>
        inline static std::tuple<Args...> create_tuple(
            vm_execution_registers* registers)
        {
            static constexpr auto size =
                std::tuple_size<std::tuple<Args...>>::value;

            static_assert(
                size < 16,
                "Attempted to register a function with more than 16 arguments");

            return create_tuple<std::tuple<Args...>>(
                registers, std::make_index_sequence<size>());
        }

        template <typename F, typename Tuple, size_t... I>
        inline static auto call_with_tuple(F func, Tuple& t,
                                           std::index_sequence<I...>)
        {
            return func(std::get<I>(t)...);
        }

        template <typename F, typename Tuple>
        inline static auto call_with_tuple(const F& func, Tuple& t)
        {
            static constexpr auto size = std::tuple_size<Tuple>::value;
            return call_with_tuple<F, Tuple>(func, t,
                                             std::make_index_sequence<size>{});
        }

        // template <typename F, typename R, typename... Args>
        // inline static void function_wrapper(vm_execution_registers*
        // registers)
        // {
        //     F f;

        //     auto tup = create_tuple<Args...>(registers);
        //     if constexpr (std::is_same_v<R, void>)
        //     {
        //         call_with_tuple(f, tup);
        //     }
        //     else
        //     {
        //         register_manip::set_register(registers[15],
        //                                      call_with_tuple(f, tup));
        //     }
        // }
    };
}  // namespace minivm

#define MINIVM_BIND(program, func) \
    minivm::program_binding::set_external_function<func>(program, #func)

#define MINIVM_BIND_VARIABLE(program, type, name) \
    type* name = program.get_extern_ptr<type>(#name)