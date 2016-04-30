hplang
------

Henrik Paananen -- henrik.j.paananen@student.jyu.fi

The language design has been infuenced by
[Jonathan Blow's language, jai](http://www.youtube.com/user/jblow888/videos)

The language will be procedural with:
- static typing
- simple type inference

    // these are equivalent:
    a : s64 = 0;
    a := 0;

- control structures:
    - if, for, while
- a few primitive datatypes:
    - signed & unsigned integers of sizes 8, 16, 32 and 64 bits
        - s8, s16, s32, s64, u8, ...
    - boolean (bool)
    - 8 bit character (char)
    - raw pointers (char*, s8*, ...)
- printing functionality, something like

    print("Hello, ", name, "\n");

these features have less priority:
- operator overloading
- procedures with variadic arguments
- multiple return values from procedures

    x, y := calculate_xy();

- deferred code-blocks (defer keyword):

    defer {
        <code-that is run after the scope ends>
    }

- possibility to define structures containing other types
- floating point numbers of sizes 32 and 64
- string datatype
- array datatype
- compile time code execution (for meta programming)


Sample programs are found in "samples" directory.

Testing happens with regularly compiling and running some test programs.
Test harness and test programs are in "tests" directory.

Target language: AMD64 for Windows (and maybe Linux).
(I may change the target language to LLVM.)

Host language: C/C++


    ; A simple program that prints the team name.
    ; Written for AMD64 on Windows (for NASM).

    global main
    extern puts
    section .text

    main:
        ; prologue
        push    rsp                 ; stack is aligned by 16
        mov     rbp,    rsp
        sub     rsp,    0x20        ; allocate 32 bytes "shadow space" needed by
                                    ; called functions

        lea     rcx,    [team_name]
        call    puts                ; print team name

        mov     eax,    0           ; return 0
        ; epilogoue
        mov     rsp,    rbp
        pop     rbp
        ret

    team_name:
        db "hplang", 0

