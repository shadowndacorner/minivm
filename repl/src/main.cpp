#include <stdio.h>
#include <iostream>

#include <minivm/vm.hpp>

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
    return 0;
}