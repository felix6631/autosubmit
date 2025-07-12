#include "tui.h"
#include <string.h>

static tui_state_t g_tui_state = {0};

int tui_init(void) {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    // Check if terminal supports colors
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);   // Header
        init_pair(2, COLOR_BLACK, COLOR_WHITE);  // Selected
        init_pair(3, COLOR_RED, COLOR_BLACK);    // Error
        init_pair(4, COLOR_GREEN, COLOR_BLACK);  // Success
    }
    
    // Create windows
    int height = LINES;
    int width = COLS;
    
    g_tui_state.main_win = newwin(height - 3, width, 0, 0);
    g_tui_state.status_win = newwin(2, width, height - 3, 0);
    g_tui_state.input_win = newwin(1, width, height - 1, 0);
    
    if (!g_tui_state.main_win || !g_tui_state.status_win || !g_tui_state.input_win) {
        tui_cleanup();
        return -1;
    }
    
    g_tui_state.current_window = WIN_MAIN;
    g_tui_state.is_logged_in = 0;
    
    return 0;
}

void tui_cleanup(void) {
    if (g_tui_state.main_win) delwin(g_tui_state.main_win);
    if (g_tui_state.status_win) delwin(g_tui_state.status_win);
    if (g_tui_state.input_win) delwin(g_tui_state.input_win);
    endwin();
}

void tui_show_main_menu(tui_state_t* state) {
    werase(state->main_win);
    box(state->main_win, 0, 0);
    
    mvwprintw(state->main_win, 1, 2, "=== Jungol Auto Submit System ===");
    mvwprintw(state->main_win, 3, 2, "1. Login");
    mvwprintw(state->main_win, 4, 2, "2. Browse Problems");
    mvwprintw(state->main_win, 5, 2, "3. Submit Solution");
    mvwprintw(state->main_win, 6, 2, "4. View Results");
    mvwprintw(state->main_win, 7, 2, "5. Settings");
    mvwprintw(state->main_win, 8, 2, "6. Exit");
    
    if (state->is_logged_in) {
        mvwprintw(state->main_win, 10, 2, "Status: Logged in as %s", state->username);
    } else {
        mvwprintw(state->main_win, 10, 2, "Status: Not logged in");
    }
    
    mvwprintw(state->main_win, 12, 2, "Use number keys to select an option");
    
    wrefresh(state->main_win);
}

void tui_update_status(tui_state_t* state, const char* message) {
    werase(state->status_win);
    box(state->status_win, 0, 0);
    mvwprintw(state->status_win, 0, 2, "Status: %s", message);
    wrefresh(state->status_win);
}

void tui_main_loop(void) {
    int ch;
    
    tui_show_main_menu(&g_tui_state);
    tui_update_status(&g_tui_state, "Ready");
    
    while ((ch = getch()) != 'q' && ch != '6') {
        switch (ch) {
            case '1':
                // Login functionality
                tui_update_status(&g_tui_state, "Login not implemented yet");
                break;
            case '2':
                // Browse problems
                tui_update_status(&g_tui_state, "Problem browser not implemented yet");
                break;
            case '3':
                // Submit solution
                tui_update_status(&g_tui_state, "Submission not implemented yet");
                break;
            case '4':
                // View results
                tui_update_status(&g_tui_state, "Results viewer not implemented yet");
                break;
            case '5':
                // Settings
                tui_update_status(&g_tui_state, "Settings not implemented yet");
                break;
            default:
                tui_update_status(&g_tui_state, "Invalid option. Use 1-6 or 'q' to quit");
                break;
        }
        
        // Refresh main menu
        tui_show_main_menu(&g_tui_state);
    }
}