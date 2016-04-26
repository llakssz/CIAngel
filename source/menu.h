#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <3ds.h>

#define MAX_SELECTED_OPTIONS 0x10

#define COLOR_TITLE 0x0000FF
#define COLOR_NEUTRAL 0xFFFFFF
#define COLOR_SELECTED 0xFF0000
#define COLOR_BACKGROUND 0x000000

#define CONSOLE_REVERSE		CONSOLE_ESC(7m)

typedef struct ConsoleMenu {
	PrintConsole menuConsole;
} ConsoleMenu;

void init_menu(gfxScreen_t screen);
int menu_draw(const char *title, const char *footer, int back, int count, const char *options[]);
int *menu_draw_selection(const char *title, int count, const char *options[], const int *preselected);

#ifdef __cplusplus
}
#endif