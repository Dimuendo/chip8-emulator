#ifndef _CHIP_8_H
#define _CHIP_8_H

#include <stdint.h>

const unsigned int VIDEO_HEIGHT = 32;
const unsigned int VIDEO_WIDTH = 64;

class Chip8 {
    private:
        // Chip 8 has 35 opcodes that are 2 bytes long
        uint16_t opcode;

        uint8_t memory[4096];

        // 16 reigsters
        uint8_t V[16];

        // Index register I and program counter PC
        uint16_t I;
        uint16_t pc;

        // Timer registers
        uint8_t delayTimer;
        uint8_t soundTimer;

        // Stack that holds current address location before a jump, 
        //   stack pointer that points to current stack location
        uint16_t stack[16];
        uint16_t sp;

        void clearDisplay();
        void initialize();

    public:
        // Pixel graphics stored in 64x32 screen, 1 if pixel is drawn
        uint8_t gfx[VIDEO_HEIGHT * VIDEO_WIDTH];

        // Chip 8 has a HEX based keypad
        uint8_t key[16];

        // True if a draw has occured
        bool drawFlag;

        Chip8();
        ~Chip8();

        // Emulate one cycle
        void emulateCycle();

        // Load game
        bool loadGame(const char *file_path);
};

#endif
