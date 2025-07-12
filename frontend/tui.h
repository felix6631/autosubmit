#ifndef AUTOSUBMIT_TUI_H
#define AUTOSUBMIT_TUI_H

#include <ncurses.h>
#include "../core/core.h"

// TUI Window types
typedef enum {
    WIN_MAIN,
    WIN_PROBLEM_LIST,
    WIN_SUBMISSION,
    WIN_RESULTS,
    WIN_SETTINGS
} window_type_t;

// TUI State structure
typedef struct {
    WINDOW* main_win;
    WINDOW* status_win;
    WINDOW* input_win;
    window_type_t current_window;
    int selected_problem;
    char username[256];
    char password[256];
    int is_logged_in;
} tui_state_t;

// TUI Functions
int tui_init(void);
void tui_cleanup(void);
void tui_main_loop(void);
void tui_show_main_menu(tui_state_t* state);
void tui_show_problem_list(tui_state_t* state);
void tui_show_submission_form(tui_state_t* state);
void tui_show_results(tui_state_t* state);
void tui_update_status(tui_state_t* state, const char* message);
int tui_get_input(tui_state_t* state, const char* prompt, char* buffer, int max_len);

#endif // AUTOSUBMIT_TUI_H