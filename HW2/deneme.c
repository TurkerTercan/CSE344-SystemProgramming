#include <stdio.h>
int main(){
    double L[] = {-6, 35, -84, 105, -70, 21};
    double x[] = {1, 2, 3, 4, 5, 6};
    double y[] = {1, 4, 9, 16, 25, 36};
    double poly[] = {0, 0, 0, 0, 0, 0};

    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            poly[j] += y[i] * L[j];
        }
    }

    for (int i = 0; i < 6; ++i) {
        printf("%.1f ", poly[i]);
    }
}