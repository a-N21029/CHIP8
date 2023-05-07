#include <stdio.h>
#include <SDL2/SDL.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("TESTING");

    SDL_Init (SDL_INIT_EVERYTHING);

    SDL_Window *window = SDL_CreateWindow("Hello World", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Event windowEvent;

    if (NULL == window)
    {
        printf("Could not create window");
        return 1;
    }
    while (1) {
        if (SDL_PollEvent (&windowEvent)) {
            if (SDL_QUIT == windowEvent.type)
            {
                break;
            }
        }
    }
    SDL_DestroyWindow (window);
    SDL_Quit();
    return 0;
}