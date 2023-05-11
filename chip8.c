#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <SDL2/SDL.h>

// SDL VARIABLES
char* TITLE = "CHIP-8";
uint16_t WIDTH = 64;
uint16_t HEIGHT = 32;
uint8_t FG_COLOUR = 0xFF; // WHITE
uint8_t BG_COLOUR = 0x00; // BLACK
uint8_t SCALE_FACTOR = 20;

// CHIP-8 SPECIFICATIONS
uint8_t ram[4096]; // 4kb RAM
bool display[64*32]; // CHIP-8 display was 64 x 32 pixels, with each pixel being either on (true/white) or off (false/black)
bool keyboard[16]; // CHIP-8 Keyboard is a hex keyboard
uint16_t stack[12]; // The original RCA 1802 version allocated 48 bytes for up to 12 levels of nesting
uint8_t cur_stack; // points to the current stack
uint8_t data_registers[16]; // CHIP-8 has 16 8-bit data registers V0-VF 
uint16_t I; // 12-bit address register used with several opcodes that involve memory operations
uint16_t PC; // store memory address of instruction to be executed next
typedef struct {
    uint16_t opcode;
    uint16_t NNN; // 12-bit address
    uint8_t NN;  // 8-bit constant
    uint8_t N;  // 4-bit constant
    uint8_t X; // 4-bit register identifier
    uint8_t Y;// 4-bit register identifier
} instruction;

// BOTH TIMERS COUNT DOWN at 60Hz, UNTIL THEY REACH 0
uint8_t delay_timer; // used for timing the events of game. can be set and read
uint8_t sound_timer; // used for sound effects, a beeping sound is made when this is not zero

bool keyboard_input[16]; // input is done with a hex keyboard with keys ranging from 0-F

char* rom; // name of the rom that will be executed

uint16_t BEGIN_LOCATION = 512; // original CHIP-8 occupies first 512 bytes, so most programs start at memory location 512, this convention will be followed here
uint8_t fonts[] = { // Hex representation of hex characters that are 4 pixels wide and 5 pixels tall
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1      Example representation of 0:
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3                11110000 <-- 0xF0
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4                10010000 <-- 0x90
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5                10010000
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6                10010000
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7                11110000
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9       If you look at the outline
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A       of the 1's you can see the 0
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// return success status of CHIP-8 initialization
bool init_chip8(char* rom_file)
{
    // load the fonts into the memory, starting from address 0x00
    memcpy(&ram[0], fonts, sizeof(fonts));

    // open ROM, whose name can be passed in from the command line
    FILE *rom = fopen(rom_file, "rb");
    if (!rom)
    {
        SDL_Log("Could not find file %s", rom_file);
        return false;
    }
    // get file size
    fseek(rom, 0, SEEK_END);
    long file_size = ftell(rom);

    long max_size = sizeof(ram) - BEGIN_LOCATION;
    rewind(rom); // rewind to beginning of file, don't want to be at the end of file because of above fseek

    if(file_size > max_size) // file is bigger than memory, can't load it
    {
        SDL_Log("File %s is too large. ROM size: %ld, Max size: %ld\n", rom_file, file_size, max_size);
        return false;
    }

    // load ROM
    if (fread(&ram[BEGIN_LOCATION], file_size, 1, rom) != 1)
    {
        SDL_Log("Could not load file %s into memory\n", rom_file);
        return false;
    }
    
    PC = BEGIN_LOCATION;
    // initialize stack pointer to 0
    cur_stack = 0;
    fclose(rom);
    return true;
}

void execute_instruction(instruction *instr)
{
    // fetch instruction from RAM
    uint16_t big_endian_opcode = ram[PC] << 8 | ram[PC + 1]; // this was done on x64 architecture which is little endian. Needed to convert opcode to big endian to match CHIP-8 system specs.
    instr->opcode = big_endian_opcode;
    PC += 2; // increment PC by 2 to start at next instruction (1 instruction is 2 bytes)

    // instruction format
    instr->N= instr->opcode & 0x0F;
    instr->NN= instr->opcode & 0x0FF;
    instr->NNN = instr->opcode & 0x0FFF;
    instr->X = (instr->opcode >> 8)& 0x0F;
    instr->Y = (instr->opcode >> 4) & 0x0F;
    // printf("%04X %04X\n", PC ,instr->opcode);
    // Emulate opcodes
    switch((instr->opcode >> 12) & 0x0F){ // mask off first number in opcode
        case 0x0:
            if(instr->NN == 0xE0){ // 00E0 (clear the screen)
                memset(&display[0], false, sizeof(display));
            }
            else if (instr->NN == 0xEE) // 00EE (return from subroutine)
            {
                cur_stack--; // pop the subroutine from the stack
                PC = stack[cur_stack]; // PC now points to next instruction in the stack
                // printf("Return from subroutine to address 0x%04X\n",
                    //    (stack[cur_stack - 1]));
            }
            break;
        case 0x01: // 1NNN (jump to address NNN)
            PC = instr->NNN;
            // printf("Jump to address NNN (0x%04X)\n",
                //    instr->NNN);  
            break;

        case 0x02: // only 0x02 instruction is 2NNN (call subroutine at address NNN)
            stack[cur_stack] = PC; // save current PC so we can return to it later
            cur_stack++; // increment stack pointer by one
            PC = instr->NNN; // jump to subroutine
            // printf("Call subroutine at NNN (0x%04X)\n",
            //        instr->NNN);
            break;
        case 0x03: // 3XNN (Skip next instruction if VX == NN)
            PC += data_registers[instr->X] == instr->NN ? 2 : 0; // skip to the next instruction
            break;
        case 0x04: // 4XNN (Skip next instruction if VX != NN)
            PC += data_registers[instr->X] != instr->NN ? 2 : 0; // skip to the next instruction
            break;
        case 0x05: // 5XY0 (Skip next instruction if VX == VY)
            PC += data_registers[instr->X] == data_registers[instr->Y]? 2 : 0; // skip to the next instruction
            break;
        case 0x06: // 6XNN (Sets VX to NN)
            data_registers[instr->X] = instr->NN;
            // printf("Set register V%X = NN (0x%02X)\n",
            //        instr->X, instr->NN);
                   break;
        case 0x07: // 7XNN (Adds NN to)
            data_registers[instr->X] += instr->NN; 
            // printf("Set register V%X (0x%02X) += NN (0x%02X). Result: 0x%02X\n",
            //        instr->X, data_registers[instr->X], instr->NN,
            //        data_registers[instr->X] + instr->NN);
            break;
        case 0x08:
            switch(instr->N){// there are many 0x08 instructions, need further decoding than just the first nibble
                case 0x00:
                    data_registers[instr->X] = data_registers[instr->Y]; // 8XY0 (VX is set to the value of VY)
                    break;
                case 0x01:
                    data_registers[instr->X] |= data_registers[instr->Y]; // 8XY1 (VX is set to VX OR VY)
                    break;
                case 0x02:
                    data_registers[instr->X] &= data_registers[instr->Y]; // 8XY2 (VX is set to VX AND VY)
                    break;
                case 0x03:
                    data_registers[instr->X] ^= data_registers[instr->Y]; // 8XY3 (VX is set to VX XOR VY)
                    break;
                case 0x04:
                    data_registers[instr->X] += data_registers[instr->Y]; // 8XY4 (VX is set to VX + VY)
                    data_registers[0xF] = data_registers[instr->X] < data_registers[instr->Y]; // if an overflow ocurred, set carry flag VF to 1, otherwise set it to 0
                    break;
                case 0x05: // 8XY5 (VX is set to VX - VY)
                    data_registers[0xF] = data_registers[instr->X] > data_registers[instr->Y]; // set carry flag to 1 if no underflow will occur, otherwise set to 0
                    data_registers[instr->X] -= data_registers[instr->Y];
                    break;
                case 0x06: // 8XY6 (VX is set to VY >> 1)
                    data_registers[0x0F] = data_registers[instr->X] & 1; // set carry flag to the bit that will be shifted out
                    data_registers[instr->X] = data_registers[instr->Y] >> 1;
                    break;
                case 0x07: // 8XY7 (VX is set to VY - VX)
                    data_registers[0xF] = data_registers[instr->X] < data_registers[instr->Y]; // set carry flag to 1 if no underflow will occur, otherwise set to 0
                    data_registers[instr->X] = data_registers[instr->Y] - data_registers[instr->X];
                    break;
                case 0x0E: // 8XYE (VX is set to VY >> 1)
                    data_registers[0x0F] = data_registers[instr->X] & 0x80; // set carry flag to the bit that will be shifted out
                    data_registers[instr->X] = data_registers[instr->Y] << 1;
                    break;
            }
            break;
        case 0x09: // 9XY0 (Skip next instruction if VX != VY)
            PC += data_registers[instr->X] != data_registers[instr->Y]? 2 : 0; // skip to the next instruction
            break;
        case 0x0A: // ANNN (Set I to address NNN)
            I = instr->NNN;
            // printf("Set I to NNN (0x%04X)\n",
            //        instr->NNN);
            break;
        case 0x0B: // BNN (Jump to address NNN plus V))
            PC = instr->NNN + data_registers[0];
            break;
        case 0x0C: // CXNN (Generate a random number, binary AND it with NN, and store result in vX)
            data_registers[instr->X] = (rand() % instr->NN) & instr->NN;  
            break;
        case 0x0D: ;// DXYN (Display: draw a sprite at coordinate (VX, VY). sprite details mentioned below)
            // get coordinates
            uint8_t orig_x = data_registers[instr->X] & (WIDTH - 1); // if coordinates exceed window width/height, wrap them around with modulo (aka bitwise AND)
            uint8_t y = data_registers[instr->Y] & (HEIGHT - 1);
            data_registers[0xF] = 0; // initialize carry flag to 0

            // Sprite has N rows, loop over them
            for(int i = 0; i < instr->N; i++)
            {
                uint8_t sprite_data = ram[I + i];
                uint8_t x = orig_x;

                // for each row:
                for(int j = 7; j >= 0; j--){ // each row has 8 pixels
                    bool pixel = display[y*WIDTH + x]; // convert 2D coordinates to 1D for the array
                    if((sprite_data & (1 << j)) && pixel){ // if current pixel in the sprite row is on and the pixel at coordinates X,Y is on, turn off the pixel and set VF to 1
                        data_registers[0xF] = 1; // set flag register to 1 if both the current sprite pixel and pixel at VX VY is set to true
                    }
                    // XOR the sprite pixel with the display pixel
                    display[y*WIDTH + x] ^= sprite_data & (1 << j);

                    // stop drawing if we hit the right edge of the sreen
                    if(++x > WIDTH){
                        break;
                    }
                }
                // stop drawing if we hit the bottom edge of the sreen
                if (++y >= WIDTH){
                        break;
                    }
            }

            // printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) "
            //        "from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n",
            //        instr->N, instr->X, data_registers[instr->X], instr->Y,
            //        data_registers[instr->Y], I);
            break;
        case 0x0E:
            switch (instr->NN)
            {
            case 0x9E: // EX9E (Skip one instruction if key corresponding to value VX is pressed)
                PC += (keyboard[data_registers[instr->X]]) ? 2 : 0;
            case 0xA1: // EXA1 (Skip one instruction if key corresponding to value VX is not pressed)
                PC += (!keyboard[data_registers[instr->X]]) ? 2 : 0;
            default:
                break;
            }
        case 0x0F:
        switch (instr->NN)
        {
            case 0x07: // FX07 (set VX to the current value of the delay timer)
                data_registers[instr->X] = delay_timer;
                break;
            case 0x15: // FX07 (set delay timer to the current value of the VX)
                delay_timer = data_registers[instr->X];
                break;
            case 0x18: // FX07 (set sound timer to the current value of the VX)
                sound_timer = data_registers[instr->X];
                break;
            case 0x1E: // FX1E (Add the value of VX to index register I)
                I += data_registers[instr->X]; // in the original COSMAC VIP, VF was not set even if there was overflow here
                break;
            case 0x0A: ;// FX0A (Stop executing instructions and wait until key input is received)
                bool key_pressed = false;
                uint8_t key;
                for (unsigned int i = 0; i < sizeof(keyboard); i++){ // loop through keys and check if any of them have been pressed
                    if (keyboard[i]){
                        key_pressed = true;
                        key = i; // store the pressed key, we will need it below
                        break;
                    }
                }

                if(!key_pressed){
                    PC -= 2;
                }
                else{
                    if(keyboard[key]){ // key is still being pressed, wait until it is released
                        PC -= 2;
                    }
                    else{ // pressed key has been released, store it in VX
                        data_registers[instr->X] = key;
                    }
                }
                break;
            case 0x29: // FX29 (I is set to the address of the hexadecimal character in VX)
                I = data_registers[instr->X] * 5; // * 5 because one font sprite occupies 5 bytes of RAM
                break;
            case 0x33: ;// FX33 (Take number in VX and convert to threed separate decimal digits and store them in ram at address I, I + 1, and I + 2, respectively)
                uint8_t n1,n2,n3;
                n1 = instr->X / 100 % 10;
                n2 = instr->X / 10 % 10;
                n3 = instr->X % 10;
                ram[I] = n1; ram[I + 1] = n2; ram[I + 2] = n3;
                break;
            case 0x55: // FX55 (Store V0...VX at address I...I+X, respectively)
                for (int i = 0; i <= instr->X; i++)
                {
                     // two possible behaviours, increment I as we go, or don't. modern CHIP-8 interpreters don't, so this was the design chosen here.
                     // could make some configuration for this part it is possible to also play older games
                    ram[I + i] = data_registers[i];
                }
            case 0x65: // FX55 (Load into V0...VX the values at address I...I+X, respectively)
                for (int i = 0; i <= instr->X; i++)
                {
                     // two possible behaviours, increment I as we go, or don't. modern CHIP-8 interpreters don't, so this was the design chosen here.
                     // could make some configuration for this part it is possible to also play older games
                     data_registers[i] = ram[I + i];
                }

            default:
                break;
            }
            break;
        default:
            break; // unimplemented/invalid opcode
    }
}

// return success status of SDL initialization
bool init_SDL(void){
    if(SDL_Init (SDL_INIT_EVERYTHING))
    {
        SDL_Log("Could not initialize SDL system %s\n", SDL_GetError());
        return false; // failure
    }
    return true; // success
}

SDL_Window *create_window(void){
    SDL_Window *window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH * SCALE_FACTOR, HEIGHT * SCALE_FACTOR, SDL_WINDOW_ALLOW_HIGHDPI);

    if (NULL == window)
    {
        printf("Could not create window %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    return window;
}

// update changes to the window
void update_screen(SDL_Renderer *renderer){
    SDL_Rect rect = {.x=0, .y = 0, .w=SCALE_FACTOR, .h=SCALE_FACTOR}; // scale chip8 pixels by SCALE_FACTOR and draw them in SDL
    
    for (unsigned int i = 0; i < sizeof(display); i++)
    {
        rect.x = (i % WIDTH) * SCALE_FACTOR;
        rect.y = (i / WIDTH) * SCALE_FACTOR;
        // printf("rectangle coordinates: %d %d\n", rect.x, rect.y);

        if (display[i]){ // pixel is on, draw foreground color
            SDL_SetRenderDrawColor(renderer, FG_COLOUR, FG_COLOUR, FG_COLOUR, SDL_ALPHA_OPAQUE);
        }
        else{ // pixel is off, draw background color
            SDL_SetRenderDrawColor(renderer, BG_COLOUR, BG_COLOUR, BG_COLOUR, SDL_ALPHA_OPAQUE);
        }
        SDL_RenderFillRect(renderer, &rect); 
    }
    SDL_RenderPresent(renderer);
}

// STILL NEEDS SOME WORK
void handle_input(void)
{
    SDL_Event windowEvent;
    while (SDL_PollEvent (&windowEvent)) {
        switch (windowEvent.type) {
            case SDL_QUIT: // USER CLOSED THE APP
                exit(EXIT_SUCCESS);
            // map qwerty keys to CHIP8 keypad
            case SDL_KEYDOWN:
                switch(windowEvent.key.keysym.sym)
                {
                    case SDLK_1: 
                        keyboard[0x1] = true;
                        break;
                    case SDLK_2: 
                        keyboard[0x2] = true;
                        break;
                    case SDLK_3: 
                        keyboard[0x3] = true;
                        break;
                    case SDLK_4: 
                        keyboard[0xC] = true;
                        break;
                    case SDLK_q: 
                        keyboard[0x4] = true;
                        break;
                    case SDLK_w: 
                        keyboard[0x5] = true;
                        break;
                    case SDLK_e: 
                        keyboard[0x6] = true;
                        break;
                    case SDLK_r: 
                        keyboard[0xD] = true;
                        break;
                    case SDLK_a: 
                        keyboard[0x7] = true;
                        break;
                    case SDLK_s: 
                        keyboard[0x8] = true;
                        break;
                    case SDLK_d: 
                        keyboard[0x9] = true;
                        break;
                    case SDLK_f: 
                        keyboard[0xE] = true;
                        break;
                    case SDLK_z: 
                        keyboard[0xA] = true;
                        break;
                    case SDLK_x: 
                        keyboard[0x0] = true;
                        break;
                    case SDLK_c: 
                        keyboard[0xB] = true;
                        break;
                    case SDLK_v: 
                        keyboard[0xF] = true;
                        break;
                    default:
                        break;
                }

            case SDL_KEYUP: // return keys back to false when they are no longer being pressed
                switch(windowEvent.key.keysym.sym)
                {
                    case SDLK_1: 
                        keyboard[0x1] = false;
                        break;
                    case SDLK_2: 
                        keyboard[0x2] = false;
                        break;
                    case SDLK_3: 
                        keyboard[0x3] = false;
                        break;
                    case SDLK_4: 
                        keyboard[0xC] = false;
                        break;
                    case SDLK_q: 
                        keyboard[0x4] = false;
                        break;
                    case SDLK_w: 
                        keyboard[0x5] = false;
                        break;
                    case SDLK_e: 
                        keyboard[0x6] = false;
                        break;
                    case SDLK_r: 
                        keyboard[0xD] = false;
                        break;
                    case SDLK_a: 
                        keyboard[0x7] = false;
                        break;
                    case SDLK_s: 
                        keyboard[0x8] = false;
                        break;
                    case SDLK_d: 
                        keyboard[0x9] = false;
                        break;
                    case SDLK_f: 
                        keyboard[0xE] = false;
                        break;
                    case SDLK_z: 
                        keyboard[0xA] = false;
                        break;
                    case SDLK_x: 
                        keyboard[0x0] = false;
                        break;
                    case SDLK_c: 
                        keyboard[0xB] = false;
                        break;
                    case SDLK_v: 
                        keyboard[0xF] = false;
                        break;
                    default:
                        break;
                }
                break;
            default: break;
        }
    }
}

void clear_screen(SDL_Renderer *renderer) {

    SDL_SetRenderDrawColor(renderer, BG_COLOUR, BG_COLOUR, BG_COLOUR, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
}

void update_timers(SDL_AudioDeviceID audio) {
    if (delay_timer > 0) 
        delay_timer--;

    if (sound_timer > 0) {
        sound_timer--;
        SDL_PauseAudioDevice(audio, 0); // Play sound
    } else {
        SDL_PauseAudioDevice(audio, 1); // Pause sound
    }
}

void audio_callback(void *userdata, uint8_t *stream, int len) {
    instruction *some_var = (instruction *) userdata; // userdata isnt needed here but need to use it so there is no error in compilation
    for (int i=0;i<len;i++) {
        some_var->N = 1;
        stream[i] = 1;
    }
}

int main(int argc, char **argv) {
    if(argc < 2)
    {
        fprintf(stderr, "Error encountered\n");
        exit(EXIT_FAILURE);
    }
    // Initialize SDL
    if(!init_SDL()){
        exit(EXIT_FAILURE);
    }

    // Initialize CHIP-8 Machine
    if(!init_chip8(argv[1]))
    {
        return 1;
    }

    // Create the window
    SDL_Window *window = create_window();

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    SDL_memset(&want, 0, sizeof(want)); /* or SDL_zero(want) */
    want.freq = 48000;
    want.format = AUDIO_F32;
    want.channels = 1; // only need 1 sound channel for CHIP-8
    want.samples = 4096;
    want.callback = audio_callback;  // you wrote this function elsewhere.
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);

    // Set initial Screen Colour
    SDL_SetRenderDrawColor(renderer, BG_COLOUR, BG_COLOUR, BG_COLOUR, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    instruction instr;
    // Main Loop
    while (true) {
        // handle user input
        handle_input();
        uint64_t start_time = SDL_GetPerformanceCounter();
        execute_instruction(&instr); // fetch, decode, and execute instruction from RAM
        uint64_t end_time = SDL_GetPerformanceCounter();

        uint64_t instr_time = ((end_time - start_time) * 1000) / SDL_GetPerformanceFrequency();

        // Delay by roughly 60 FPS
        SDL_Delay(16.67 > instr_time ? 16.67 - instr_time : 0); // 1/16ms = 60Hz = 60FPS (technically should be 16.6666... but only accept ints)

        // Update window with changes
        update_screen(renderer);  
        update_timers(dev);

    }

    // Cleanup in the end
    SDL_DestroyWindow (window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return 0;
}