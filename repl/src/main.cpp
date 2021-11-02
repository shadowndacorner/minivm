#include <stdio.h>
#include <iostream>

#include <minivm/vm.hpp>
#include <minivm/vm_binding.hpp>

static void externVoidFunc()
{
    printf("extern test\n");
}

static float externIntFunc(int test, float test2)
{
    printf("extern test %d %f\n", test, test2);
    return test2 / test;
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        fprintf(stderr, "No input file specified");
        return 1;
    }

    minivm::program program;
    if (!program.load_assembly_from_file(argv[1]))
    {
        fprintf(stderr, "Failed to load assembly from file: %s\n",
                program.get_load_error());
        return 2;
    }

    // minivm::vm_execution_registers regs;
    // minivm::program_binding::wrapper_fn_generator<void>::call<test>(&regs);

    MINIVM_BIND_VARIABLE(program, double, externVar);
    if (externVar)
    {
        *externVar = 350;
    }

    MINIVM_BIND_FUNCTION(program, externVoidFunc);
    MINIVM_BIND_FUNCTION(program, externIntFunc);

    minivm::execution_context executor(program);
    if (!executor.run_from("main"))
    {
        if (executor.get_error())
        {
            std::cerr << executor.get_error() << std::endl;
            return 3;
        }
    }

    while (executor.did_yield() && executor.resume())
    {
    }

    if (executor.get_error())
    {
        std::cerr << executor.get_error() << std::endl;
        return 3;
    }

    if (externVar)
    {
        printf("Final value of external variable was %f\n", *externVar);
    }
    return 0;
}