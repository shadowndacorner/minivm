#include <stdio.h>
#include <iostream>

#include <minivm/vm.hpp>
#include <minivm/vm_binding.hpp>

static void test()
{
    printf("extern test\n");
}

static float testint(int test, float test2)
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

    double* externVar;
    if (program.get_extern_ptr("externVar", &externVar))
    {
        *externVar = 2000.0;
    }

    minivm::program_binding::set_external_function<test>(program,
                                                         "externVoidFunc");

    minivm::program_binding::set_external_function<testint>(program,
                                                            "externIntFunc");

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