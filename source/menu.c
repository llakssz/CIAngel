/* Code borrowed from https://github.com/mid-kid/CakesForeveryWan/blob/master/source/menu.c and tortured until it bent to my will */
#include "menu.h"

#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include "utils.h"

int selected_options[MAX_SELECTED_OPTIONS];

ConsoleMenu currentMenu;

void init_menu(gfxScreen_t screen)
{
    // Create our new console, initialize it, and switch back to the previous console
    currentMenu.menuConsole = *consoleGetDefault();
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);
    consoleInit(screen, &currentMenu.menuConsole);

    consoleSelect(currentConsole);
}

void menu_draw_string(const char* str, int pos_x, int pos_y, const char* color)
{
    currentMenu.menuConsole.cursorX = pos_x;
    currentMenu.menuConsole.cursorY = pos_y;
    printf("%s%s%s", color, str, CONSOLE_RESET);

    gfxFlushBuffers();
}

void menu_draw_string_full(const char* str, int pos_y, const char* color)
{
    currentMenu.menuConsole.cursorX = 0;
    currentMenu.menuConsole.cursorY = pos_y;
    printf(color);

    if (currentMenu.menuConsole.consoleWidth == 50)
    {
        printf("%-50s", str);
    }
    else
    {
        printf("%-40s", str);
    }
    printf(CONSOLE_RESET);

    gfxFlushBuffers();
}

int menu_draw(const char *title, const char* footer, int back, int count, const char *options[])
{
    int selected = 0;

    // Select our menu console and clear the screen
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);
    consoleClear();

    int current = 0;
    int pos_y_text[count];
    int current_pos_y = 0;

    // Draw the header
    menu_draw_string(title, 0, current_pos_y++, CONSOLE_RED);

    // Draw the menu
    pos_y_text[0] = current_pos_y;
    menu_draw_string(options[0], 1, current_pos_y++, CONSOLE_REVERSE);
    // Don't allow the menu to draw beyond the edge of the screen, just truncate if so
    for (int i = 1; i < count && i < (currentMenu.menuConsole.consoleHeight - 2); i++) {
        pos_y_text[i] = current_pos_y;
        menu_draw_string(options[i], 1, current_pos_y++, CONSOLE_WHITE);
    }

    // Draw the footer if one is provided
    if (footer != NULL)
    {
        current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
        menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
    }

    while (true) {
        u32 key = wait_key();
        
        if (key & KEY_UP) {
            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_WHITE);

            if (current <= 0) {
                current = count - 1;
            } else {
                current--;
            }

            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_REVERSE);
        } else if (key & KEY_DOWN) {
            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_WHITE);

            if (current >= count - 1) {
                current = 0;
            } else {
                current++;
            }

            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_REVERSE);
        } else if (key & KEY_RIGHT) {
            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_WHITE);

            current += 5;
            if (current >= count) current = count - 1;

            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_REVERSE);
        } else if (key & KEY_LEFT) {
            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_WHITE);
            
            current -= 5;
            if (current < 0) current = 0;

            menu_draw_string(options[current], 1, pos_y_text[current], CONSOLE_REVERSE);
        } else if (key & KEY_A) {
            selected = current;
            break;
        } else if ((key & KEY_B) && back) {
            selected = -1;
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);
    return selected;
}

int *menu_draw_selection(const char *title, int count, const char *options[], const int *preselected)
{
    // The caller has to make sure it does not exceed MAX_SELECTED_OPTIONS
    // if (count > MAX_SELECTED_OPTIONS) {
    //     return NULL;
    // }

    // memset(selected_options, 0, sizeof(selected_options));

    // int current = 0;
    // int pos_x_text = 4 * SPACING_HORIZ;
    // int pos_y_text[count];
    // int current_pos_y = 30;

    // clear_screen(GFX_TOP);
    // draw_string(GFX_TOP, title, 0, 0, COLOR_TITLE);

    // pos_y_text[0] = current_pos_y;
    // draw_string(GFX_TOP, "[ ]", 0, current_pos_y, COLOR_NEUTRAL);
    // current_pos_y = draw_string(GFX_TOP, options[0], pos_x_text, current_pos_y, COLOR_SELECTED);
    // int i;
    // for (i = 1; i < count; i++) {
    //     current_pos_y += SPACING_VERT;
    //     pos_y_text[i] = current_pos_y;
    //     draw_string(GFX_TOP, "[ ]", 0, current_pos_y, COLOR_NEUTRAL);
    //     current_pos_y = draw_string(GFX_TOP, options[i], pos_x_text, current_pos_y, COLOR_NEUTRAL);
    // }
    // draw_string(GFX_TOP, "Press START to confirm", 0, current_pos_y + SPACING_VERT * 2, COLOR_SELECTED);

    // for (int i = 0; i < count; i++) {
    //     if (preselected[i]) {
    //         draw_character(GFX_TOP, 'x', 0 + SPACING_HORIZ, pos_y_text[i], COLOR_NEUTRAL);
    //         selected_options[i] = 1;
    //     }
    // }

    // while (1) {
    //     u16 key = wait_key();

    //     if (key & KEY_UP) {
    //         draw_string(GFX_TOP, options[current], pos_x_text, pos_y_text[current], COLOR_NEUTRAL);

    //         if (current <= 0) {
    //             current = count - 1;
    //         } else {
    //             current--;
    //         }

    //         draw_string(GFX_TOP, options[current], pos_x_text, pos_y_text[current], COLOR_SELECTED);
    //     } else if (key & KEY_DOWN) {
    //         draw_string(GFX_TOP, options[current], pos_x_text, pos_y_text[current], COLOR_NEUTRAL);

    //         if (current >= count - 1) {
    //             current = 0;
    //         } else {
    //             current++;
    //         }

    //         draw_string(GFX_TOP, options[current], pos_x_text, pos_y_text[current], COLOR_SELECTED);
    //     } else if (key & KEY_A) {
    //         if (selected_options[current]) {
    //             draw_character(GFX_TOP, 'x', 0 + SPACING_HORIZ, pos_y_text[current], COLOR_BACKGROUND);
    //             selected_options[current] = 0;
    //         } else {
    //             draw_character(GFX_TOP, 'x', 0 + SPACING_HORIZ, pos_y_text[current], COLOR_NEUTRAL);
    //             selected_options[current] = 1;
    //         }
    //     } else if ((key & KEY_START) || (key & KEY_B)) {
    //         return selected_options;
    //     }
    // }
    return 0;
}
