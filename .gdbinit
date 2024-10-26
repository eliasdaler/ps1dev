source gdb/eastl-prettyprint.py

define target remote
target extended-remote $arg0
symbol-file build/game.elf
monitor reset shellhalt
load build/game.elf
add-symbol-file ~/work/pcsx-redux/src/mips/openbios/openbios.elf
end

set max-value-size unlimited
set print asm-demangle
target remote localhost:3333
tui new-layout horizontal-asm {-horizontal src 1 asm 1} 2 status 0 cmd 1
layout horizontal-asm

# b Game.cpp:621
# b Renderer.cpp:363
# b Model.cpp:27
# b main.cpp:482
# b main.cpp:546
# b drawTris
# b multiplyMatrix33
b main.cpp:554
# b alloc.c:90
c
