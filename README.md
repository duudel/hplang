
hplang
======

Author
-----------------------------

Henrik Juhana Paananen
henrik.j.paananen@student.jyu.fi



Credits
-------

Basic features
* Data types:
    - empty type:               void
    - boolean:                  bool
    - single character:         char
    - signed integers:          s8, s16, s32, s64
    - unsigned integers:        u8, u16, u32, u64
    - floating point numbers:   f32, f64
    - string:                   string
    - pointer types:            void*, char*, ...
    - function types:           !(f64, f64) : bool
* Integer arithmetic
* Control structures: if, for, while
    - Break and continue can be used to break from or countinue to the next
      loop iteration in for and while loops.
* Procedures with parameters

-------
= 3op

Other features
* Function overloads                    0.5op
* "First-class functions"               0.5op
* Aggregate types + type aliases        0.5op
* Simple type "inference"               0.5op
* Intermediate language generation      1op
* Symbolic machine code generation
    and "smart" register allocation     2op

-------
= 5op

Total credits = 8op => 6op


Compiling the hplang compiler
-----------------------------

The compiler can be compiled with gcc that supports c++11. Compiling the
compiler has been tested on Linux (Arch Linux) and Windows (Msys2) The project
can be compiled by issuing

> make

or

> make build

This will build the hplang stdandard library into stdlib/libstdlib.a, and then
it will build the compiler into ./hplangc (./hplangc.exe on Windows).

Running test suite:

> make run_tests


Using the compiler
------------------

hplang [options] <source>
  compile <source> into binary executable

options:
  --output <filename>
	-o <filename>             Sets the output filename
  --target <target>
	-T <target>               Sets the output target
<target> can be one of [win64|win_amd64|elf64|linux64]

  --diagnostic [memory|ast|ir|regalloc]
	-dMAiR                    Selects the diagnostic options
  --profile [instrcount]
	-pi                       Selects profiling options
  --help
	-h                        Shows this help and exits
  --version
	-v                        Prints the version information

When no output filename is given (-o/--output) "out" will be used.  

When diagnostic options are given (-d/--diagnostic) various information is
written to the standard error stream. In addition, when "regalloc" diagnostic
option is specified, out.is.s file is written. The file contains the target
code after instruction selection and before register allocation.

Total instruction count emitted (before optimizations and after optimizations)
can be measured with --profile instrcount (-pi).

Example:

> ./hplangc -o out samples/fibo.hp
> ./out
> fibo(10) = 55
> fibo2(10) = 55


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

