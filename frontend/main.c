#include "tui.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Initializing Jungol Auto Submit TUI...\n");
    
    if (tui_init() != 0) {
        fprintf(stderr, "Failed to initialize TUI\n");
        return 1;
    }
    
    tui_main_loop();
    tui_cleanup();
    
    printf("Goodbye!\n");
    return 0;
}