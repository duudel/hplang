
hplang
======

Compiling the hplang compiler
-----------------------------

The compiler can be compiled with gcc that supports c++11. Compiling the
compiler has been tested on Linux (Arch Linux) and Windows (Msys2) The project
can be compiled by issuing

    make

or

    make build

This will build the hplang stdandard library into stdlib/libstdlib.a, and then
it will build the compiler into ./hplangc (./hplangc.exe on Windows).

Running test suite:

    make run_tests


Using the compiler
------------------

The compiler assumes that nasm (for assembling) and gcc (for linking) have been
installed and are invokable for the compiler (i.e. in system search path).

    hplang [options] <source>
      compile <source> into binary executable

    options:
      --output <filename>
        -o <filename>             Sets the output filename
      --target <target>
        -T <target>               Sets the output target
    <target> can be one of [win64|win_amd64|elf64|linux64]

      --diagnostic [memory|ast|ir|regalloc]
        -dMAIR                    Selects the diagnostic options
      --profile [time|instrcount]
        -pti                      Selects profiling options
      --help
        -h                        Shows this help and exits
      --version
        -v                        Prints the version information

When no output filename is given (-o/--output) "out" will be used.

When diagnostic options are given (-d/--diagnostic) various information is
written to the standard error stream. In addition, when "regalloc" diagnostic
option is specified, out.is.s file is written. The file contains the target
code after instruction selection and before register allocation.

Timing of different compilation phases can be measured with "--profile time" or
"-pt".  Total instruction count emitted (before optimizations and after
optimizations) can be measured with "--profile instrcount" or "-pi".


The compiler outputs out.s (independent of the source filename)  containing the
generated symbolic machine code.  Next the compiler invokes nasm to assemble
the asm file into and object file out.o. Lastly gcc is invoked to link the
final binary.


Example:

    ./hplangc -o out samples/fibo.hp
    ./out
    fibo(10) = 55
    fibo2(10) = 55


Other files
-----------

    LANGUAGE                    Gives a brief description of the hplang.

    Sample programs
    samples/beer.hp             Prints 99 bottles lyrics.
    samples/factorial.hp        Prints the factorial of some numbers.
    samples/fibo.hp             Prints the Fibonacci numbers recursively and iteratively.
    samples/hello.hp            Hello world.
    samples/mandelbrot.hp       Prints Mandelbrot fractal.
    samples/mandelbrot_other.hp Prints Mandelbrot fractal.
    samples/nbody.hp            Simulates gravitation on planets. (Warning: takes ~20s to finish).
    samples/syntax_ideas.hp     Not a compilable sample; contains notes about syntax ideas.
    samples/compiletime.hp      Not a compilable sample; contains notes about syntax ideas.

    Test suite
    tests/tests.cpp             The main test program.

    Standard library
    stdlib/stdlib.c             The hplang standard library.
    stdlib/io.hp                The hplang standard io module.

Author
-----------------------------

Henrik Paananen

henrik.j.paananen@student.jyu.fi

