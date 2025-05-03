// main.c
#include "image_processing.h"

#define MAX_PATH 256

int main() {
    FILE *log = fopen("log.txt", "w");
    if (!log) {
        perror("No se pudo abrir el log");
        return 1;
    }

    for (int i = 1; i <= 100; i++) {
        char entrada[MAX_PATH];
        char salida_gris[MAX_PATH];
        char salida_color[MAX_PATH];

        snprintf(entrada, MAX_PATH, "./images/image%d.bmp", i);
        snprintf(salida_gris, MAX_PATH, "./processed/image%d_grises.bmp", i);
        snprintf(salida_color, MAX_PATH, "./processed/image%d_color.bmp", i);

        invertirHorizontalGrises(entrada, salida_gris, log);
        invertirHorizontalColor(entrada, salida_color, log);
    }

    fclose(log);
    return 0;
}
