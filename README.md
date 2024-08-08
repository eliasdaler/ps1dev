# PS1 dev

PS1 development experiments.

# Requirements

For now, only Linux build is supported.

1. [PCSX-Redux](https://github.com/grumpycoders/pcsx-redux)
2. CMake
3. gdb-multiarch
3. GCC MIPS toolchain
4. Download PsyQ converted libs from [here](http://psx.arthus.net/sdk/Psy-Q/psyq-4.7-converted-full.7z) and put it in `<cloned_repo_dir>/../psyq`. Alternatively you can set the PsyQ dir via `-DPSYQ_DIR` when generating CMake files.

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
pcsx-redux -exe build/game.ps-exe -run
```

Run game and debug in gdb:

```sh
./run_and_debug_game.sh
```
