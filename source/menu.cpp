/* Code borrowed from https://github.com/mid-kid/CakesForeveryWan/blob/master/source/menu.c and tortured until it bent to my will */
#include "menu.h"

#include <string>
#include <vector>
#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include "utils.h"

int selected_options[MAX_SELECTED_OPTIONS];

void init_menu(gfxScreen_t screen)
{
    // Create our new console, initialize it, and switch back to the previous console
    currentMenu.menuConsole;
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);
    consoleInit(screen, &currentMenu.menuConsole);

    consoleSelect(currentConsole);
}

ConsoleMenu currentMenu;

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
void titles_multkey_draw(const char *title, const char* footer, int back, std::vector<game_item> *options, void* data,
                      bool (*callback)(int result, u32 key, void* data))

{
    // Select our menu console and clear the screen
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);
    
    int count = options->size();
    bool firstLoop = true;
    int current = 0;
    int previous_index = 0;
    int menu_offset = 0;
    int previous_menu_offset = 1;
    int menu_pos_y;
    int menu_end_y = currentMenu.menuConsole.consoleHeight -1; 
    int current_pos_y = 0;

    while (aptMainLoop()) {
        if(firstLoop || previous_index != current) {
            firstLoop = false;
            int results_per_page = menu_end_y - menu_pos_y;
            int current_page = current / results_per_page;
            menu_offset = current_page * results_per_page;
            char prev_entry[50];
            char entry[50];
            sprintf(prev_entry, "%-*.*s (%s) %*.*s",
                31, 31, (*options)[previous_index].name.c_str(),
                (*options)[previous_index].region.c_str(),
                10, 10, (*options)[previous_index].code.c_str());
            sprintf(entry, "%-*.*s (%s) %*.*s",
                31, 31, (*options)[current].name.c_str(),
                (*options)[current].region.c_str(),
                10, 10, (*options)[current].code.c_str());
            if(menu_offset == previous_menu_offset) {
                menu_draw_string(prev_entry, 1, menu_pos_y + (previous_index-menu_offset), CONSOLE_WHITE);
                menu_draw_string(entry, 1, menu_pos_y + (current-menu_offset), CONSOLE_REVERSE);
            } else {
                consoleClear();
                current_pos_y=0;
                // Draw the header
                menu_draw_string(title, 0, current_pos_y++, CONSOLE_RED);
                menu_pos_y = current_pos_y;
                for (int i = 0; menu_offset + i < count && i < results_per_page; i++) {
                    sprintf(entry, "%-*.*s (%s) %*.*s",
                        31, 31, (*options)[menu_offset + i].name.c_str(),
                        (*options)[menu_offset + i].region.c_str(),
                        10, 10, (*options)[menu_offset + i].code.c_str());
                    if(i+menu_offset == current) {
                        menu_draw_string(entry, 1, current_pos_y, CONSOLE_REVERSE);
                    } else {
                        menu_draw_string(entry, 1, current_pos_y, CONSOLE_WHITE);
                    }
                    current_pos_y++;
                    previous_menu_offset = menu_offset;
                }
                if (footer != NULL)
                {
                    // Draw the footer if one is provided
                    current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
                    menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
                }
            }
            previous_index = current;
        }
        u32 key = wait_key();
        
        if (key & KEY_UP) {
            if (current <= 0) {
                current = count - 1;
            } else {
                current--;
            }
        } else if (key & KEY_DOWN) {
            if (current >= count - 1) {
                current = 0;
            } else {
                current++;
            }
        } else if (key & KEY_RIGHT) {
            current += 5;
            if (current >= count) current = count - 1;
        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0) current = 0;
        } else if (callback(current, key, data)) {
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);
}

void menu_multkey_draw(const char *title, const char* footer, int back, int count, const char *options[], void* data,
                      bool (*callback)(int result, u32 key, void* data))

{
    // Select our menu console and clear the screen
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);

    int current = 0;
    int previous_index = 1;
    int menu_offset = 0;
    int previous_menu_offset = 1;
    int menu_pos_y;
    int menu_end_y = currentMenu.menuConsole.consoleHeight -2; 
    int current_pos_y = 0;

    while (aptMainLoop()) {
        if(previous_index != current) {
            int results_per_page = menu_end_y - menu_pos_y;
            int current_page = current / results_per_page;
            menu_offset = current_page * results_per_page;
            if(menu_offset == previous_menu_offset) {
                menu_draw_string(options[menu_offset + previous_index], 1, menu_pos_y + (previous_index-menu_offset), CONSOLE_WHITE);
                menu_draw_string(options[menu_offset + current], 1, menu_pos_y + (current-menu_offset), CONSOLE_REVERSE);
            } else {
                consoleClear();
                current_pos_y=0;
                // Draw the header
                menu_draw_string(title, 0, current_pos_y++, CONSOLE_RED);
                menu_pos_y = current_pos_y;
                for (int i = 0; (menu_offset + i) < count && i < results_per_page; i++) {
                    if(i+menu_offset == current) {
                        menu_draw_string(options[menu_offset + i], 1, current_pos_y, CONSOLE_REVERSE);
                    } else {
                        menu_draw_string(options[menu_offset + i], 1, current_pos_y, CONSOLE_WHITE);
                    }
                    current_pos_y++;
                    previous_menu_offset = menu_offset;
                }
                if (footer != NULL)
                {
                    // Draw the footer if one is provided
                    current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
                    menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
                }
            }
            previous_index = current;
        }
        u32 key = wait_key();
        
        if (key & KEY_UP) {
            if (current <= 0) {
                current = count - 1;
            } else {
                current--;
            }
        } else if (key & KEY_DOWN) {
            if (current >= count - 1) {
                current = 0;
            } else {
                current++;
            }
        } else if (key & KEY_RIGHT) {
            current += 5;
            if (current >= count) current = count - 1;
        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0) current = 0;
        } else if (callback(current, key, data)) {
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);
}
