#include "app.h"
#include "tui.h"
#include <stdio.h>
#include <math.h>

int app_calc_main(void) {
    tui_print_header("BangOS Geometric Calculator");

    double side1 = 0.0, side2 = 0.0, hypotenuse = 0.0;

    printf(ANSI_GREEN "Enter first side: " ANSI_RESET);
    fflush(stdout);
    if (scanf("%lf", &side1) != 1) {
        printf(ANSI_RED "Error: invalid number input.\n" ANSI_RESET);
        // Clear input line
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        tui_pause();
        return 1;
    }

    printf(ANSI_GREEN "Enter second side: " ANSI_RESET);
    fflush(stdout);
    if (scanf("%lf", &side2) != 1) {
        printf(ANSI_RED "Error: invalid number input.\n" ANSI_RESET);
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        tui_pause();
        return 1;
    }

    // Clear trailing newline
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    hypotenuse = sqrt(side1 * side1 + side2 * side2);
    double area = (side1 * side2) / 2.0;
    double perimeter = side1 + side2 + hypotenuse;

    tui_print_divider();
    printf(ANSI_BOLD "Calculation Results:\n" ANSI_RESET);
    printf("  - Side A:     %.2f\n", side1);
    printf("  - Side B:     %.2f\n", side2);
    printf("  - Hypotenuse: " ANSI_BOLD ANSI_GREEN "%.2f" ANSI_RESET "\n", hypotenuse);
    printf("  - Area:       %.2f\n", area);
    printf("  - Perimeter:  %.2f\n", perimeter);
    tui_print_divider();

    tui_pause();
    return 0;
}
