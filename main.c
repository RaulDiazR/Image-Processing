// main.c
#include "image_processing.h"
#include <time.h>
#include <string.h>
#include <omp.h>
#include <locale.h>

#define NUM_THREADS 18
#define NUM_IMAGENES 100

void formatNumberWithCommas(const char *numStr, char *buffer);

int main() {
    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS); // Se definen los threads a utilizar
    FILE *log = fopen("log.txt", "w");
    if (!log) {
        perror("No se pudo abrir el archivo de log");
        return 1;
    }

    int kernelSize;
    while (1) {
        printf("Ingrese el tamano del kernel (impar entre 3 y 155): ");
        scanf("%d", &kernelSize);
    
        if (kernelSize >= 3 && kernelSize <= 155 && kernelSize % 2 != 0) {
            break; // valid input, exit loop
        }
    
        printf("Tamano invalido. Debe ser un numero impar entre 3 y 155.\n\n");
    }

    unsigned long long totalLecturas = 0;
    unsigned long long totalEscrituras = 0;

    clock_t start = clock();

    for (int i = 1; i <= NUM_IMAGENES; i++) {
        char entrada[256];
        snprintf(entrada, sizeof(entrada), "./images/image%d.bmp", i);

        char salidaHorizontalGrises[256];
        snprintf(salidaHorizontalGrises, sizeof(salidaHorizontalGrises), "./processed/image%d_horizontal_gris.bmp", i);

        char salidaHorizontalColor[256];
        snprintf(salidaHorizontalColor, sizeof(salidaHorizontalColor), "./processed/image%d_horizontal_color.bmp", i);

        char salidaVerticalGrises[256];
        snprintf(salidaVerticalGrises, sizeof(salidaVerticalGrises), "./processed/image%d_vertical_gris.bmp", i);

        char salidaVerticalColor[256];
        snprintf(salidaVerticalColor, sizeof(salidaVerticalColor), "./processed/image%d_vertical_color.bmp", i);

        char salidaDesenfoque[256];
        snprintf(salidaDesenfoque, sizeof(salidaDesenfoque), "./processed/image%d_blur_kernelSize%d.bmp", i, kernelSize);

        char salidaGrises[256];
        snprintf(salidaGrises, sizeof(salidaGrises), "./processed/image%d_gris.bmp", i);
        
        unsigned long lecturasHorizontalGrises = 0, escriturasHorizontalGrises = 0;
        unsigned long lecturasHorizontalColor = 0, escriturasHorizontalColor = 0;
        unsigned long lecturasVerticalGrises = 0, escriturasVerticalGrises = 0;
        unsigned long lecturasVerticalColor = 0, escriturasVerticalColor = 0;
        unsigned long lecturasBlur = 0, escriturasBlur = 0;
        unsigned long lecturasGrises = 0, escriturasGrises = 0;

        invertirHorizontalGrises(entrada, salidaHorizontalGrises, log, &lecturasHorizontalGrises, &escriturasHorizontalGrises);
        invertirHorizontalColor(entrada, salidaHorizontalColor, log, &lecturasHorizontalColor, &escriturasHorizontalColor);
        invertirVerticalGrises(entrada, salidaVerticalGrises, log, &lecturasVerticalGrises, &escriturasVerticalGrises);
        invertirVerticalColor(entrada, salidaVerticalColor, log, &lecturasVerticalColor, &escriturasVerticalColor);
        aplicarDesenfoqueIntegral(entrada, salidaDesenfoque, kernelSize, log, &lecturasBlur, &escriturasBlur);
        convertirAGrises(entrada, salidaGrises, log, &lecturasGrises, &escriturasGrises);

        totalLecturas += lecturasHorizontalGrises + lecturasHorizontalColor;
        totalEscrituras += escriturasHorizontalGrises + escriturasHorizontalColor;
        totalLecturas += lecturasGrises;

        totalLecturas += lecturasVerticalGrises + lecturasVerticalColor;
        totalEscrituras += escriturasVerticalGrises + escriturasVerticalColor;

        totalLecturas += lecturasBlur * (kernelSize * kernelSize);
        totalEscrituras += escriturasBlur;
        totalEscrituras += escriturasGrises;

        printf("\rProcesando imagenes: %d/%d completadas...", i, NUM_IMAGENES);
        fflush(stdout);
    }

    clock_t end = clock();
    double tiempoTotal = (double)(end - start) / CLOCKS_PER_SEC;

    unsigned long long totalAccesos = totalLecturas + totalEscrituras;
    /* Se realiza el cálculo de instrucciones, utilizando el total de accesos,
    factor de 20.0 por líneas de código en ensamblador, número de threads y área del kernel */
    double instrucciones = (long double)totalAccesos * 20.0 * NUM_THREADS;
    // Se calcula el millón de instrucciones por segundo
    double mips = (instrucciones / 1e6) / tiempoTotal;
    // Calculamos MB por segundo
    double velocidadMBps = 31341.36 / tiempoTotal;
    
    char lecturasStr[32], escriturasStr[32], bufferLecturas[32], bufferEscrituras[32];
    char mipsStr[64], formattedMips[64];
    int minutos = (int)(tiempoTotal / 60);
    double segundos = tiempoTotal - (minutos * 60);  
    
    sprintf(lecturasStr, "%llu", totalLecturas);
    sprintf(escriturasStr, "%llu", totalEscrituras);
    sprintf(mipsStr, "%.2f", mips);
    formatNumberWithCommas(lecturasStr, bufferLecturas);
    formatNumberWithCommas(escriturasStr, bufferEscrituras);
    formatNumberWithCommas(mipsStr, formattedMips);
    
    fprintf(log, "\n--- Reporte Final ---\n");
    fprintf(log, "Kernel utilizado para desenfoque: %d\n", kernelSize);
    fprintf(log, "Total de localidades leídas: %s\n", bufferLecturas);
    fprintf(log, "Total de localidades escritas: %s\n", bufferEscrituras);
    fprintf(log, "Tiempo total de ejecución: %d minutos con %.2f segundos\n", minutos, segundos);
    fprintf(log, "Velocidad de procesamiento: %.2f MB/s\n", velocidadMBps);
    fprintf(log, "MIPS estimados: %s\n", formattedMips);

    fclose(log);

    printf("\n\nProcesamiento terminado.\n");
    printf("Tiempo total: %d minutos con %.2f segundos\n", minutos, segundos);
    printf("MIPS estimados: %s\n", formattedMips);

    return 0;
}

void formatNumberWithCommas(const char *numStr, char *buffer) {
    char intPart[64], decPart[64] = "";
    char temp[64];

    const char *dot = strchr(numStr, '.');
    if (dot) {
        size_t intLen = dot - numStr;
        strncpy(intPart, numStr, intLen);
        intPart[intLen] = '\0';
        strncpy(decPart, dot + 1, sizeof(decPart) - 1);
    } else {
        strcpy(intPart, numStr);
    }

    int len = strlen(intPart);
    int commas = (len - 1) / 3;
    int newLen = len + commas;
    temp[newLen] = '\0';

    int i = len - 1;
    int j = newLen - 1;
    int count = 0;

    while (i >= 0) {
        temp[j--] = intPart[i--];
        count++;
        if (count == 3 && i >= 0) {
            temp[j--] = ',';
            count = 0;
        }
    }

    if (strlen(decPart) > 0) {
        snprintf(buffer, 64, "%s.%s", temp, decPart);
    } else {
        strcpy(buffer, temp);
    }
}