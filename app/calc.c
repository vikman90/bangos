#include <stdio.h>
#include <math.h>

int main(void) {
    double side1, side2, hypotenuse;

    printf("Enter first side: ");
    fflush(stdout);
    if (scanf("%lf", &side1) != 1) {
        printf("Error: invalid input.\n");
        return 1;
    }

    printf("Enter second side: ");
    fflush(stdout);
    if (scanf("%lf", &side2) != 1) {
        printf("Error: invalid input.\n");
        return 1;
    }

    hypotenuse = sqrt(side1 * side1 + side2 * side2);

    printf("Hypotenuse: %.2f\n", hypotenuse);

    return 0;
}
