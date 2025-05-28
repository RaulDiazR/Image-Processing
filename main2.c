#include "image_processing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define NUM_THREADS 18
#define NUM_IMAGENES 5  // Cambia este número si deseas procesar más o menos imágenes

void formatNumberWithCommas(const char *numStr, char *buffer);

unsigned long long calcularTamanoTotalProcesados(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char fullPath[512];
    unsigned long long totalSize = 0;

    dir = opendir(path);
    if (!dir) {
        perror("No se pudo abrir el directorio de salida");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
        if (stat(fullPath, &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
            totalSize += fileStat.st_size;
        }
    }

    closedir(dir);
    return totalSize;
}

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS);

    FILE *log = fopen("log.txt", "w");
    if (!log) {
        perror("No se pudo abrir el archivo de log");
        return 1;
    }

    int kernelSize = 55;
    if (argc > 1) {
        kernelSize = atoi(argv[1]);
        if (kernelSize < 3 || kernelSize > 155 || kernelSize % 2 == 0) {
            fprintf(stderr, "Kernel inválido. Usando 55 por defecto.\n");
            kernelSize = 55;
        }
    }

    unsigned long long totalLecturas = 0, totalEscrituras = 0;
    clock_t start = clock();

    for (int i = 1; i <= NUM_IMAGENES; i++) {
        char entrada[256], salidaHorizontalGrises[256], salidaHorizontalColor[256];
        char salidaVerticalGrises[256], salidaVerticalColor[256], salidaDesenfoque[256], salidaGrises[256];

        snprintf(entrada, sizeof(entrada), "./images/image%d.bmp", i);
        snprintf(salidaHorizontalGrises, sizeof(salidaHorizontalGrises), "./processed/image%d_horizontal_gris.bmp", i);
        snprintf(salidaHorizontalColor, sizeof(salidaHorizontalColor), "./processed/image%d_horizontal_color.bmp", i);
        snprintf(salidaVerticalGrises, sizeof(salidaVerticalGrises), "./processed/image%d_vertical_gris.bmp", i);
        snprintf(salidaVerticalColor, sizeof(salidaVerticalColor), "./processed/image%d_vertical_color.bmp", i);
        snprintf(salidaDesenfoque, sizeof(salidaDesenfoque), "./processed/image%d_blur_kernelSize%d.bmp", i, kernelSize);
        snprintf(salidaGrises, sizeof(salidaGrises), "./processed/image%d_gris.bmp", i);

        unsigned long l1 = 0, e1 = 0, l2 = 0, e2 = 0, l3 = 0, e3 = 0;
        unsigned long l4 = 0, e4 = 0, l5 = 0, e5 = 0, l6 = 0, e6 = 0;

        invertirHorizontalGrises(entrada, salidaHorizontalGrises, log, &l1, &e1);
        invertirHorizontalColor(entrada, salidaHorizontalColor, log, &l2, &e2);
        invertirVerticalGrises(entrada, salidaVerticalGrises, log, &l3, &e3);
        invertirVerticalColor(entrada, salidaVerticalColor, log, &l4, &e4);
        aplicarDesenfoqueIntegral(entrada, salidaDesenfoque, kernelSize, log, &l5, &e5);
        convertirAGrises(entrada, salidaGrises, log, &l6, &e6);

        totalLecturas += l1 + l2 + l3 + l4 + (l5 * kernelSize * kernelSize) + l6;
        totalEscrituras += e1 + e2 + e3 + e4 + e5 + e6;

        printf("\rProcesando imagenes: %d/%d completadas...", i, NUM_IMAGENES);
        fflush(stdout);
    }

    clock_t end = clock();
    double tiempoTotal = (double)(end - start) / CLOCKS_PER_SEC;

    double instrucciones = (double)(totalLecturas + totalEscrituras) * 20.0 * NUM_THREADS;
    double mips = (instrucciones / 1e6) / tiempoTotal;

    unsigned long long bytes = calcularTamanoTotalProcesados("./images");
    double MBs = ((double)bytes * 6) / (1024.0 * 1024.0);
    double velocidadMBps = MBs / tiempoTotal;

    char lecturasStr[32], escriturasStr[32], bufferLecturas[32], bufferEscrituras[32];
    char mipsStr[64], formattedMips[64];
    int minutos = (int)(tiempoTotal / 60);
    double segundos = tiempoTotal - minutos * 60;

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

    int i = len - 1, j = newLen - 1, count = 0;
    while (i >= 0) {
        temp[j--] = intPart[i--];
        if (++count == 3 && i >= 0) {
            temp[j--] = ',';
            count = 0;
        }
    }

    if (strlen(decPart) > 0)
        snprintf(buffer, 64, "%s.%s", temp, decPart);
    else
        strcpy(buffer, temp);
}
