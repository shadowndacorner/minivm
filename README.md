# MiniVM

This is a small C++17 register VM.  I wrote primarily as a learning exercise, but intend to use it for a statically typed scripting language I am developing separately.

## Usage

### Build Integration

MiniVM has only been tested on Windows with clang and MSVC, but it should be supported by any C++17-compatible compiler.  If you use cmake, simply add this repository as a subdirectory (or use something like CPM) and link against the static `minivm` target.  If you use another build system, simply add `vm/include` to your include directories and add all files under `vm/src/` to your build process.

It has not been tested on 32 bit systems.  32 bit support is not currently planned, though may be implemented if the need arises.

### Usage Example

See `repl/main.cpp` for a usage example, as well as `samples/sample.mvma` to see an example assembly file that can be loaded.  The intention is to eventually support a binary assembly format as well, but currently text-based assembly is all that is supported.


## Overview
> A note on type safety: Because the target scripting language is statically typed, the VM makes no guarantees regarding type safety.  All registers are 64 bits and may be interpreted as signed/unsigned integers or double precision floats, howeve the VM has no idea what type is stored in any register at a given time.

There are two high level objects involved in using MiniVM - the `program`, and the `execution_context`.

As you might expect, a `program` contains a set of instructions and labels.  It also contains a table of `externs` - 64 bit values that can be written to/read by the host application.  The `externs` table also contains external function pointers.  MiniVM makes it very easy to bind external functions with two restrictions on what functions may be bound:

1. The function must take no more than 16 arguments
2. All arguments must be signed/unsigned integer types, floating point types, or pointers
3. The return type must be `void` or a valid argument type

To bind an external function, simply include `<minivm/vm_binding.hpp>` and call `MINIVM_BIND_FUNCTION(program, myFunction)`.  This macro internally does some template magic to generate a wraper function that takes a pointer to the VM register state, converts it to a typed tuple with the necessary arguments, then unpacks that tuple and passes the parameters to `myFunction`.  This whole operation has very minimal overhead, especially compared to external calls in dynamically typed languages.

External values may similarly be bound with `MINIVM_BIND_VARIABLE(program, type, variableName)`, which creates a pointer to the program's external variable named variableName of the provided type.  The type must be a `double`, `uint64_t`, `int64_t`, or a pointer.

### TODO: Add more here.  This is incomplete.