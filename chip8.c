#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define W 64
#define H 32
#define PS 8

//#define DEBUG_OPCODE_NAME
//#define DEBUG_FPS
#define LIMIT_FPS

static u_int8_t mem[4096] = {0};
static u_int8_t v[16] = {0};
static u_int16_t i = 0;
static u_int16_t pc = 0; // program counter
static u_int8_t sp = 0;  // stack pointer
static u_int8_t dt = 0;  // delay timer
static u_int8_t st = 0;  // sount timer
static u_int16_t stack[16] = {0};
static u_int16_t keys[16] = {0};
static u_int8_t framebuffer[W * H] = {0};
static SDL_Event e;
static SDL_Renderer *renderer;
static const u_int32_t FPS = 60;

struct Opcode {
  u_int16_t mask;
  void (*fn)(u_int16_t);
  u_int16_t code;
  char name[4];
};

u_int8_t DrawPixel(u_int8_t x, u_int8_t y) {
  int pos = y * W + x;
  int tmp = framebuffer[pos];
  framebuffer[pos] ^= 1;
  /* printf("%d\n", framebuffer[pos]); */
  if (framebuffer[pos] == tmp)
    return 0;
  return 1;
}

u_int8_t KeysymToKey(u_int16_t key) {
  switch (key) {
  case SDLK_0:
    return 0;
  case SDLK_1:
    return 1;
  case SDLK_2:
    return 2;
  case SDLK_3:
    return 3;
  case SDLK_4:
    return 4;
  case SDLK_5:
    return 5;
  case SDLK_6:
    return 6;
  case SDLK_7:
    return 7;
  case SDLK_8:
    return 8;
  case SDLK_9:
    return 9;
  case SDLK_a:
    return 10;
  case SDLK_b:
    return 11;
  case SDLK_c:
    return 12;
  case SDLK_d:
    return 13;
  case SDLK_e:
    return 14;
  case SDLK_f:
    return 15;
  default:
    return 0xff;
  }
}

u_int16_t argNNN(u_int16_t op) { return op & 0x0FFF; }

u_int8_t argNN(u_int16_t op) { return op & 0x00FF; }

u_int8_t argN(u_int16_t op) { return op & 0x000F; }

u_int8_t argX(u_int16_t op) { return ((op & 0x0F00) >> 8); }

u_int8_t argY(u_int16_t op) { return ((op & 0x00F0) >> 4); }

void op0NNN(u_int16_t op) {return;}

void op00E0(u_int16_t op) {
  for (int j = 0; j < W * H; j++) {
    framebuffer[j] = 0;
  }
}

void op00EE(u_int16_t op) { pc = stack[sp--]; }

void op1NNN(u_int16_t op) { pc = argNNN(op); }

void op2NNN(u_int16_t op) {
  stack[++sp] = pc;
  pc = argNNN(op);
}

void op3XNN(u_int16_t op) {
  if (v[argX(op)] == argNN(op)) {
    pc += 2;
  }
}

void op4XNN(u_int16_t op) {
  if (v[argX(op)] != argNN(op)) {
    pc += 2;
  }
}

void op5XY0(u_int16_t op) {
  if (v[argX(op)] == v[argY(op)]) {
    pc += 2;
  }
}

void op6XNN(u_int16_t op) { v[argX(op)] = argNN(op); }

void op7XNN(u_int16_t op) { v[argX(op)] += argNN(op); }

void op8XY0(u_int16_t op) { v[argX(op)] = v[argY(op)]; }

void op8XY1(u_int16_t op) { v[argX(op)] |= v[argY(op)]; }

void op8XY2(u_int16_t op) { v[argX(op)] &= v[argY(op)]; }

void op8XY3(u_int16_t op) { v[argX(op)] ^= v[argY(op)]; }

void op8XY4(u_int16_t op) {
  u_int16_t sum = (u_int16_t)v[argX(op)] + (u_int16_t)v[argY(op)];
  v[argX(op)] = (u_int8_t)sum;
  v[0xF] = (sum > 0xFF) ? 1 : 0;
}

void op8XY5(u_int16_t op) {
  v[0xF] = (v[argX(op)] > v[argY(op)]) ? 1 : 0;
  v[argX(op)] -= v[argY(op)];
}

void op8XY6(u_int16_t op) {
  v[0xF] = v[argX(op)] & 0x1;
  v[argX(op)] >>= 1;
}

void op8XY7(u_int16_t op) {
  v[0xF] = (v[argX(op)] < v[argY(op)]) ? 1 : 0;
  v[argX(op)] = v[argY(op)] - v[argX(op)];
}

void op8XYE(u_int16_t op) {
  v[0xF] = (v[argX(op)] >> 7) & 0x1;
  v[argX(op)] <<= 1;
}

void op9XY0(u_int16_t op) {
  if (v[argX(op)] != v[argY(op)]) {
    pc += 2;
  }
}

void opANNN(u_int16_t op) { i = argNNN(op); }

void opBNNN(u_int16_t op) { pc = (argNNN(op) + v[0x0]); }

void opCXNN(u_int16_t op) { v[argX(op)] = (rand() % 256) & argNN(op); }

void opDXYN(u_int16_t op) {
  u_int8_t x = v[argX(op)] % W;
  u_int8_t y = v[argY(op)] % H;
  for (u_int8_t j = 0; j < argN(op); j++) {
    u_int8_t sprite = mem[i + j];
    for (u_int8_t z = 0; z < 8; z++) {
      if ((sprite >> (7-z)) & 0x1) {
        v[0xF] += DrawPixel(x + z, y + j);
      }
    }
  }
  v[0xF] = (v[0xF] > 0) ? 1 : 0;

}

void opEX9E(u_int16_t op) {
  if (keys[v[argX(op)]]) {
    pc += 2;
  }
}

void opEXA1(u_int16_t op) {
  if (!keys[v[argX(op)]]) {
    pc += 2;
  }
}

void opFX07(u_int16_t op) { v[argX(op)] = dt; }

void opFX0A(u_int16_t op) {
  SDL_PollEvent(&e);
  if (e.type == SDL_KEYDOWN) {
      u_int8_t key = KeysymToKey(e.key.keysym.sym);
      if (key <= 0xf) {
          v[argX(op)] = key;
          return;
      }
  }
  pc -= 2;
}

void opFX15(u_int16_t op) { dt = v[argX(op)]; }

void opFX18(u_int16_t op) { st = v[argX(op)]; }

void opFX1E(u_int16_t op) { i += v[argX(op)]; }

void opFX29(u_int16_t op) { i = 0x0 + v[argX(op)] * 5; }

void opFX33(u_int16_t op) {
  u_int8_t num = v[argX(op)];
  mem[i] = num / 100;
  mem[i + 1] = num % 100 / 10;
  mem[i + 2] = num % 10;
}

void opFX55(u_int16_t op) {
  for (int j = 0; j <= argX(op); j++) {
    mem[i + j] = v[j];
  }
}

void opFX65(u_int16_t op) {
  for (int j = 0; j <= argX(op); j++) {
    v[j] = mem[i + j];
  }
}

static const struct Opcode OPCODES[35] = {
    {0xFFFF, op00E0, 0x00E0, "00E0"}, {0xFFFF, op00EE, 0x00EE, "00EE"},
    {0xF000, op0NNN, 0x0000, "0NNN"}, {0xF000, op1NNN, 0x1000, "1NNN"},
    {0xF000, op2NNN, 0x2000, "2NNN"}, {0xF000, op3XNN, 0x3000, "3XNN"},
    {0xF000, op4XNN, 0x4000, "4XNN"}, {0xF00F, op5XY0, 0x5000, "5XY0"},
    {0xF000, op6XNN, 0x6000, "6XNN"}, {0xF000, op7XNN, 0x7000, "7XNN"},
    {0xF00F, op8XY0, 0x8000, "8XY0"}, {0xF00F, op8XY1, 0x8001, "8XY1"},
    {0xF00F, op8XY2, 0x8002, "8XY2"}, {0xF00F, op8XY3, 0x8003, "8XY3"},
    {0xF00F, op8XY4, 0x8004, "8XY4"}, {0xF00F, op8XY5, 0x8005, "8XY5"},
    {0xF00F, op8XY6, 0x8006, "8XY6"}, {0xF00F, op8XY7, 0x8007, "8XY7"},
    {0xF00F, op8XYE, 0x800E, "8XYE"}, {0xF00F, op9XY0, 0x9000, "9XY0"},
    {0xF000, opANNN, 0xA000, "ANNN"}, {0xF000, opBNNN, 0xB000, "BNNN"},
    {0xF000, opCXNN, 0xC000, "CXNN"}, {0xF000, opDXYN, 0xD000, "DXYN"},
    {0xF0FF, opEX9E, 0xE09E, "EX9E"}, {0xF000, opEXA1, 0xE0A1, "EXA1"},
    {0xF0FF, opFX07, 0xF007, "FX07"}, {0xF0FF, opFX0A, 0xF00A, "FX0A"},
    {0xF0FF, opFX15, 0xF015, "FX15"}, {0xF0FF, opFX18, 0xF018, "FX18"},
    {0xF0FF, opFX1E, 0xF01E, "FX1E"}, {0xF0FF, opFX29, 0xF029, "FX29"},
    {0xF0FF, opFX33, 0xF033, "FX33"}, {0xF0FF, opFX55, 0xF055, "FX55"},
    {0xF0FF, opFX65, 0xF065, "FX65"}};

int main(int argc, char **argv) {

  mem[0] = 0xF0;
  mem[5] = 0x20;
  mem[10] = 0xF0;
  mem[15] = 0xF0;
  mem[1] = 0x90;
  mem[6] = 0x60;
  mem[11] = 0x10;
  mem[16] = 0x10;
  mem[2] = 0x90;
  mem[7] = 0x20;
  mem[12] = 0xF0;
  mem[17] = 0xF0;
  mem[3] = 0x90;
  mem[8] = 0x20;
  mem[13] = 0x80;
  mem[18] = 0x10;
  mem[4] = 0xF0;
  mem[9] = 0x70;
  mem[14] = 0xF0;
  mem[19] = 0xF0;

  mem[20] = 0x90;
  mem[25] = 0xF0;
  mem[30] = 0xF0;
  mem[35] = 0xF0;
  mem[21] = 0x90;
  mem[26] = 0x80;
  mem[31] = 0x80;
  mem[36] = 0x10;
  mem[22] = 0xF0;
  mem[27] = 0xF0;
  mem[32] = 0xF0;
  mem[37] = 0x20;
  mem[23] = 0x10;
  mem[28] = 0x10;
  mem[33] = 0x90;
  mem[38] = 0x40;
  mem[24] = 0x10;
  mem[29] = 0xF0;
  mem[34] = 0xF0;
  mem[39] = 0x40;

  mem[40] = 0xF0;
  mem[45] = 0xF0;
  mem[50] = 0xF0;
  mem[55] = 0xE0;
  mem[41] = 0x90;
  mem[46] = 0x90;
  mem[51] = 0x90;
  mem[56] = 0x90;
  mem[42] = 0xF0;
  mem[47] = 0xF0;
  mem[52] = 0xF0;
  mem[57] = 0xE0;
  mem[43] = 0x90;
  mem[48] = 0x10;
  mem[53] = 0x90;
  mem[58] = 0x90;
  mem[44] = 0xF0;
  mem[49] = 0xF0;
  mem[54] = 0x90;
  mem[59] = 0xE0;

  mem[60] = 0xF0;
  mem[65] = 0xE0;
  mem[70] = 0xF0;
  mem[75] = 0xF0;
  mem[61] = 0x80;
  mem[66] = 0x90;
  mem[71] = 0x80;
  mem[76] = 0x80;
  mem[62] = 0x80;
  mem[67] = 0x90;
  mem[72] = 0xF0;
  mem[77] = 0xF0;
  mem[63] = 0x80;
  mem[68] = 0x90;
  mem[73] = 0x80;
  mem[78] = 0x80;
  mem[64] = 0xF0;
  mem[69] = 0xE0;
  mem[74] = 0xF0;
  mem[79] = 0x80;

  if (argc < 2)
    return 1;

  srand(time(NULL));

  FILE *fp = fopen(argv[1], "rb");
  fread(&mem[512], 1, 4096 - 512, fp);
  fclose(fp);

  SDL_Window *window;

  SDL_Init(SDL_INIT_EVERYTHING);
  window =
      SDL_CreateWindow("title", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       W * PS, H * PS, SDL_WINDOW_SHOWN);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  pc = 0x200;

  u_int32_t beginTime;
  u_int32_t deltaTime;
  int run = 1;
  while (run) {
      for (int i=0;i<16 ;i++ ) {
          keys[i] = 0;
      }
    beginTime = SDL_GetTicks();
    u_int16_t op = mem[pc];
    op <<= 8;
    op |= mem[pc + 1];
    pc += 2;
    dt += (dt) ? -1 : 0;
    st += (dt) ? -1 : 0;

    for (int j = 0; j < 35; j++) {
      if ((op & OPCODES[j].mask) == OPCODES[j].code) {
         (*OPCODES[j].fn)(op);
#ifdef DEBUG_OPCODE_NAME
          printf("%s\n", OPCODES[j].name);
#endif
          break;
      }
    }

     while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
        run = 0;
        break;
      case SDL_KEYDOWN:
          keys[KeysymToKey(e.key.keysym.mod)] = 1;
          break;
      default:
        break;
      }
    }

    SDL_SetRenderDrawColor(renderer,0,0,0,0);
    SDL_RenderClear(renderer);
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        if (framebuffer[y * W + x]) {
          SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
          SDL_Rect r = {x * PS, y * PS, PS, PS};
          SDL_RenderDrawRect(renderer, &r);
          SDL_RenderFillRect(renderer, &r);
        }

      }
    }

    SDL_RenderPresent(renderer);
    deltaTime = SDL_GetTicks() - beginTime;
#ifdef DEBUG_FPS
    printf("%d\n", deltaTime);
#endif
#ifdef LIMIT_FPS
    if (deltaTime < (1000/FPS)) {
        SDL_Delay((1000/FPS) - deltaTime);
    }
#endif

  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
