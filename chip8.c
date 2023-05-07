#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <SDL2/SDL.h>

char* TITLE = "CHIP-8";
uint16_t WIDTH = 64 * 20;
uint16_t HEIGHT = 32 * 20;
uint8_t FG_COLOUR = 0xFF; // WHITE
uint8_t BG_COLOUR = 0x00; // BLACK


bool init_SDL(void){
    if(SDL_Init (SDL_INIT_EVERYTHING))
    {
        SDL_Log("Could not initialize SDL system %s\n", SDL_GetError());
        return false; // failure
    }
    return true; // success
}

SDL_Window *create_window(void){
    SDL_Window *window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);

    if (NULL == window)
    {
        printf("Could not create window %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    return window;
}

// update changes to the window
void update_screen(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);
}

void handle_input(void)
{
    SDL_Event windowEvent;
    while (SDL_PollEvent (&windowEvent)) {
        switch (windowEvent.type) {
            case SDL_QUIT:
                exit(EXIT_SUCCESS);
            case SDL_KEYDOWN: return;

            case SDL_KEYUP: return;

            default: break;
        }
    }
}


int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // Initialize SDL
    if(!init_SDL()){
        exit(EXIT_FAILURE);
    }

    // Create the window
    SDL_Window *window = create_window();

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Set initial Screen Colour
    SDL_SetRenderDrawColor(renderer, BG_COLOUR, BG_COLOUR, BG_COLOUR, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    // Main Loop
    while (true) {
        // handle user input
        handle_input();

        // Delay by roughly 60 FPS
        SDL_Delay(16); // 1/16ms = 60Hz = 60FPS (technically should be 16.6666... but only accept ints)

        // Update window with changes
        update_screen(renderer);  

    }

    // Cleanup in the end
    SDL_DestroyWindow (window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return 0;
}