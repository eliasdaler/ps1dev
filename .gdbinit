define target remote
target extended-remote $arg0
symbol-file build/game.elf
monitor reset shellhalt
load build/game.elf
add-symbol-file ~/work/pcsx-redux/src/mips/openbios/openbios.elf
end

target remote localhost:3333
c
