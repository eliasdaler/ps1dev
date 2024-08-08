#include "Game.h"

// #define _WCHAR_T
// #include <stddef.h>

/* void* operator new(size_t size)
{
    return mem_malloc(size);
}

void operator delete(void* ptr)
{
    mem_free(ptr);
}

void operator delete(void* ptr, unsigned int size)
{
    mem_free(ptr);
} */

int main()
{
    Game game;
    game.init();
    game.run();

    return 0;
}
