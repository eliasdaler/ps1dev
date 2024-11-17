source gdb/eastl-prettyprint.py

define target remote
    target extended-remote $arg0
    monitor reset shellhalt
    # TODO: don't hardcode here
    add-symbol-file ~/work/pcsx-redux/src/mips/openbios/openbios.elf
end

set max-value-size unlimited
set print asm-demangle
target remote localhost:3333
tui new-layout horizontal-asm {-horizontal src 1 asm 1} 2 status 0 cmd 1
layout horizontal-asm

# b testMatrix
b Renderer.cpp:232

c
