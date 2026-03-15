#include <SDL2/SDL.h>
#include "time_step.h"

uint64_t time_now_ms(void)
{
    return (uint64_t)SDL_GetTicks64();
}
