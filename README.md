# PS1 dev

PS1 development experiments.

# Requirements

For now, only Linux build is supported.

1. [PCSX-Redux](https://github.com/grumpycoders/pcsx-redux)
2. CMake
3. gdb-multiarch
3. MIPS toolchain

```sh
sudo apt-get install gdb-multiarch gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu
```

# How to

Build:

```sh
cmake --preset=default
cmake --build --preset=default
```

Run:

```sh
pcsx-redux -exe build_test/game.ps-exe -run
```

Run game and debug in gdb:

```sh
./run_and_debug_game.sh
```
