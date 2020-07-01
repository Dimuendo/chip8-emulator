#include <stdio.h>
#include <random>
#include <iostream>
#include "chip8.h"
#include "time.h"

// Chip 8 font set
unsigned char chip8_fontset[80] =
{ 
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

Chip8::Chip8() {}
Chip8::~Chip8() {}

void Chip8::initialize() {
    // Initialize registers and memory
    pc = 0x200;
    opcode = 0;
    I = 0;
    sp = 0;

    // Clear display
    for (int i = 0; i < 64 * 32; i++) {
        gfx[i] = 0;
    }

    // Clear stack, keypad, and registers V0-VF
    for (int i = 0; i < 16; i++) {
        stack[i] = 0;
        key[i] = 0;
        V[i] = 0;
    }

    // Clear memory
    for (int i = 0; i < 4096; i++) {
        memory[i] = 0;
    }

    // Load fontset
    for (int i = 0; i < 80; i++) {
        memory[i] = chip8_fontset[i];
    }

    // Reset timers
    delayTimer = 0;
    soundTimer = 0;

    // Seed rng
    srand(time(NULL));
}

void Chip8::clearDisplay() {
    for (int i = 0; i < 64 * 32; i++) {
        gfx[i] = 0;
    }
}

bool Chip8::loadGame(const char *file_path) {
    // Initalize
    initialize();

    printf("Loading ROM: %s\n", file_path);

    // Open the ROM
    FILE* rom = fopen(file_path, "rb");
    if (rom == NULL) {
        std::cerr << "Failed to open ROM" << std::endl;
        return false;
    }

    // Get file size
    fseek(rom, 0, SEEK_END);
    long romSize = ftell(rom);
    rewind(rom);

    // Allocate memory to store rom
    char* romBuffer = (char*) malloc(sizeof(char) * romSize);
    if (romBuffer == NULL) {
        std::cerr << "Failed to allocate memory for ROM" << std::endl;
        return false;
    }

    // Copy ROM into buffer
    size_t result = fread(romBuffer, sizeof(char), (size_t)romSize, rom);
    if (result != romSize) {
        std::cerr << "Failed to read ROM" << std::endl;
        return false;
    }

    // Copy buffer to memory
    if ((4096-512) > romSize){
        for (int i = 0; i < romSize; ++i) {
            // Load into memory - start at 0x200 (0x200 == 512)
            memory[i + 512] = (uint8_t)romBuffer[i];
        }
    }
    else {
        std::cerr << "ROM too large to fit in memory" << std::endl;
        return false;
    }

    // Clean up
    fclose(rom);
    free(romBuffer);

    return true;
}

void Chip8::emulateCycle() {
    // Fetch Opcode - opcodes are two bytes long, each memory address 
    //   contains one byte, merge two bytes to get the opcode
    unsigned short opcode = memory[pc] << 8 | memory[pc + 1]; // left shift and merge with bitwise OR

    // Decode Opcode
    switch(opcode & 0xF000) {
        case 0x0000: // 0***
            switch(opcode & 0x000F) {
                case 0x0000: // 00E0
                    // Clear the screen
                    clearDisplay();
                    drawFlag = true;
                    pc += 2;
                    break;

                case 0x000E: // 00EE
                    // Return from subroutine
                    sp--;
                    pc = stack[sp];
                    pc += 2;
                    return;
            
                default:
                    printf ("Unknown opcode [0x0000]: 0x%X\n", opcode);
                    exit(3);
            }
            break;
        
        case 0x1000: // 1NNN
            // Jump to adress NNN
            pc = opcode & 0x0FFF;
            break;

        case 0x2000: // 2NNN
            // Calls subroutine at NNN
            stack[sp] = pc;
            ++sp;
            pc = opcode & 0x0FFF;
            break;

        case 0x3000: //3XNN
            // Skips the next instruction if VX equals NN
            if (V[((opcode & 0x0F00) >> 8)] == (opcode & 0x00FF)) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0x4000:  // 4XNN
            // Skips the next instruction if VX does not equal NN
            if (V[((opcode & 0x0F00) >> 8)] != (opcode & 0x00FF)) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0x5000: // 5XY0
            // Skips the next instruction if VX equals VY
            if (V[((opcode & 0x0F00) >> 8)] == V[((opcode & 0x00F0) >> 4)]) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0x6000: // 6XNN
            // Sets VX to NN
            V[((opcode & 0x0F00) >> 8)] = opcode & 0x00FF;
            pc += 2;
            break;

        case 0x7000: // 7XNN
            // Adds NN to VX (Carry flag is not changed)
            V[((opcode & 0x0F00) >> 8)] += opcode & 0x00FF;
            pc += 2;
            break;

        case 0x8000: // 8***
            switch (opcode & 0x000F) {
                case 0x0000: // 8XY0
                    // Sets VX to the value of VY
                    V[((opcode & 0x0F00) >> 8)] = V[((opcode & 0x00F0) >> 4)];
                    pc += 2;
                    break;

                case 0x0001: // 8XY1
                    // Sets VX to VX OR VY
                    V[((opcode & 0x0F00) >> 8)] |= V[((opcode & 0x00F0) >> 4)];
                    pc += 2;
                    break;

                case 0x0002: // 8XY2
                    // Sets VX to VX AND VY
                    V[((opcode & 0x0F00) >> 8)] &= V[((opcode & 0x00F0) >> 4)];
                    pc += 2;
                    break;

                case 0x0003: // 8XY3
                    // Sets VX to VX XOR VY
                    V[((opcode & 0x0F00) >> 8)] ^= V[((opcode & 0x00F0) >> 4)];
                    pc += 2;
                    break;

                case 0x0004: // 8XY4
                    // Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
                    if (V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8])) {
                        V[0xF] = 1; // Carry
                    } else {
                        V[0xF] = 0;
                    }
                    V[((opcode & 0x0F00) >> 8)] += V[((opcode & 0x00F0) >> 4)];
                    pc += 2;
                    break;

                case 0x0005: // 8XY5
                    // VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't
                    if (V[((opcode & 0x0F00) >> 8)] >= V[((opcode & 0x00F0) >> 4)]) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0; // Borrow
                    }
                    V[((opcode & 0x0F00) >> 8)] -= V[((opcode & 0x00F0) >> 4)];
                    pc += 2;
                    break;

                case 0x0006: // 8XY6
                    // Stores the least significant bit of VX in VF and then shifts VX to the right by 1
                    V[0xF] = (V[((opcode & 0x0F00) >> 8)] & 0x1);
                    V[((opcode & 0x0F00) >> 8)] >>= 1;
                    pc += 2;
                    break;

                case 0x0007: // 8XY7
                    // Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't
                    if (V[((opcode & 0x00F0) >> 4)] >= V[((opcode & 0x0F00) >> 8)]) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0; // Borrow
                    }
                    V[((opcode & 0x0F00) >> 8)] = V[((opcode & 0x00F0) >> 4)] - V[((opcode & 0x0F00) >> 8)];
                    pc += 2;
                    break;

                case 0x000E: // 8XYE
                    // Stores the most significant bit of VX in VF and then shifts VX to the left by 1
                    V[0xF] = (V[((opcode & 0x0F00) >> 8)] & 0x1);
                    V[((opcode & 0x0F00) >> 8)] <<= 1;
                    pc += 2;
                    break;

                default:
                    printf ("Unknown opcode [0x0000]: 0x%X\n", opcode);
                    exit(3);
            }
            break;

        case 0x9000: // 9XY0
            // Skips the next instruction if VX doesn't equal VY
            if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0xA000: // ANNN
            // Sets I to the address NNN
            I = opcode & 0x0FFF;
            pc += 2;
            break;

        case 0xB000: // BNNN
            // Jumps to the address NNN plus V0
            pc = V[0x0] + (opcode & 0x0FFF);
            break;

        case 0xC000: // CXNN
            // Sets VX to the result of a bitwise and operation on a random number and NN
            V[(opcode & 0x0F00) >> 8] = (rand() % (0xFF + 1)) & (opcode & 0x00FF);
            pc += 2;
            break;

        case 0xD000: // DXYN
        {
            // Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels. 
            // Each row of 8 pixels is read as bit-coded starting from memory location I
            // I value doesn’t change after the execution of this instruction. 
            // As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn’t happen
            unsigned short x = V[(opcode & 0x0F00) >> 8];
            unsigned short y = V[(opcode & 0x00F0) >> 4];
            unsigned short height = opcode & 0x000F;
            unsigned short pixel;

            V[0xF] = 0;
            for (int yLine = 0; yLine < height; yLine++) {
                pixel = memory[I + yLine];
                for (int xLine = 0; xLine < 8; xLine++) {
                    if ((pixel & (0x80 >> xLine)) != 0) {
                        if (gfx[(x + xLine + ((y + yLine) * 64))] == 1) {
                            V[0xF] = 1;
                        }
                        gfx[x + xLine + ((y + yLine) * 64)] ^= 1;
                    }
                }
            }
            drawFlag = true;
            pc += 2;
        }
            break;

        case 0xE000: // E***
            switch(opcode & 0x00FF) {
                case 0x009E: // EX9E
                    // Skips the next instruction if the key stored in VX is pressed
                    if (key[V[(opcode & 0x0F00) >> 8]] != 0) {
                        pc += 2;
                    }
                    pc += 2;
                    break;

                case 0x00A1: // EXA1
                    // Skips the next instruction if the key stored in VX isn't pressed
                    if (key[V[(opcode & 0x0F00) >> 8]] == 0) {
                        pc += 2;
                    }
                    pc += 2;
                    break;

                default:
                    printf ("Unknown opcode [0x0000]: 0x%X\n", opcode);
                    exit(3);
            }
            break;

        case 0xF000: // F***
            switch(opcode & 0x00FF) {
                case 0x0007: // FX07
                    // Sets VX to the value of the delay timer.
                    V[(opcode & 0x0F00) >> 8] = delayTimer;
                    pc += 2;
                    break;

                case 0x000A: // FX0A
                {
                    // A key press is awaited, and then stored in VX
                    bool keyPressed = false;

                    for (int i = 0; i < 16; i++) {
                        if (key[i] != 0) {
                            V[(opcode & 0x0F00) >> 8] = i;
                            keyPressed = true;
                        }
                    }

                    // No key is pressed - return
                    if (!keyPressed) return;

                    pc += 2;
                }
                    break;

                case 0x0015: // FX15
                    // Sets the delay timer to VX
                    delayTimer = V[(opcode & 0x0F00) >> 8];
                    pc += 2;
                    break;

                case 0x0018: // FX18
                    // Sets the sound timer to VX
                    soundTimer = V[(opcode & 0x0F00) >> 8];
                    pc += 2;
                    break;

                case 0x001E: // FX1E
                    // Adds VX to I. VF is not affected
                    I += V[(opcode & 0x0F00) >> 8];
                    pc += 2;
                    break;

                case 0x0029: // FX29
                    // Sets I to the location of the sprite for the character in VX
                    // Characters 0-F (in hexadecimal) are represented by a 4x5 font
                    I = V[(opcode & 0x0F00) >> 8] * 0x5;
                    pc += 2;
                    break;

                case 0x0033: // FX33
                    // Stores the binary-coded decimal representation of VX
                    // The most significant of three digits at the address in I, 
                    // The middle digit at I plus 1, and the least significant digit at I plus 2
                    memory[I] = V[(opcode & 0x0F00) >> 8] / 100;
                    memory[I + 1] = (V[(opcode & 0x0F00) >> 8] / 10) % 10;
                    memory[I + 2] = (V[(opcode & 0x0F00) >> 8] % 100) % 10;
                    pc += 2;
                    break;

                case 0x0055: // FX55
                    // Stores V0 to VX (including VX) in memory starting at address I
                    // The offset from I is increased by 1 for each value written, but I itself is left unmodified
                    for (int i = 0; i <= V[(opcode & 0x0F00) >> 8]; i++) {
                        memory[I + i] = V[i];
                    }
                    pc += 2;
                    break;

                case 0x0065: // FX65
                    // Fills V0 to VX (including VX) with values from memory starting at address I
                    // The offset from I is increased by 1 for each value written, but I itself is left unmodified
                    for (int i = 0; i <= V[(opcode & 0x0F00) >> 8]; i++) {
                        V[i] = memory[I + i];
                    }
                    pc += 2;
                    break;

                default:
                    printf ("Unknown opcode [0x0000]: 0x%X\n", opcode);
                    exit(3);
            }
            break;

        default:
            printf("Unknown opcode: 0x%X\n", opcode);
            exit(3);
    }

    // Update timers
    if (delayTimer > 0) {
        --delayTimer;
    }

    if (soundTimer > 0) {
        --soundTimer;
    }
}
