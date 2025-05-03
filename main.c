// main.c
#include "image_processing.h"
#include <time.h>

#define NUM_IMAGENES 100

int main() {
    FILE *log = fopen("log.txt", "w");
    if (!log) {
        perror("No se pudo abrir el archivo de log");
        return 1;
    }

    unsigned long totalLecturas = 0;
    unsigned long totalEscrituras = 0;

    clock_t start = clock();

    for (int i = 1; i <= NUM_IMAGENES; i++) {
        char entrada[256];
        snprintf(entrada, sizeof(entrada), "./images/image%d.bmp", i);

        char salidaGrises[256];
        snprintf(salidaGrises, sizeof(salidaGrises), "./processed/image%d_horizontal_gris.bmp", i);

        char salidaColor[256];
        snprintf(salidaColor, sizeof(salidaColor), "./processed/image%d_horizontal_color.bmp", i);

        unsigned long lecturasGrises = 0, escriturasGrises = 0;
        unsigned long lecturasColor = 0, escriturasColor = 0;

        invertirHorizontalGrises(entrada, salidaGrises, log, &lecturasGrises, &escriturasGrises);
        invertirHorizontalColor(entrada, salidaColor, log, &lecturasColor, &escriturasColor);

        totalLecturas += lecturasGrises + lecturasColor;
        totalEscrituras += escriturasGrises + escriturasColor;

        // Actualizar solo una línea
        printf("\rProcesando imagenes: %d/%d completadas...", i, NUM_IMAGENES);
        fflush(stdout);
    }

    clock_t end = clock();
    double tiempoTotal = (double)(end - start) / CLOCKS_PER_SEC;

    // Calcular MIPS
    double instrucciones = (totalLecturas + totalEscrituras) * 20.0;
    double mips = (instrucciones / 1e6) / tiempoTotal;

    // Guardar resultados finales en el log
    fprintf(log, "\n--- Reporte Final ---\n");
    fprintf(log, "Total de localidades leídas: %lu\n", totalLecturas);
    fprintf(log, "Total de localidades escritas: %lu\n", totalEscrituras);
    fprintf(log, "Tiempo total de ejecución: %.2f segundos\n", tiempoTotal);
    fprintf(log, "MIPS estimados: %.2f\n", mips);

    fclose(log);

    printf("\n\nProcesamiento terminado.\n");
    printf("Tiempo total: %.2f segundos\n", tiempoTotal);
    printf("MIPS estimados: %.2f\n", mips);

    return 0;
}
