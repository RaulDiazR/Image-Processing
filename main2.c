#include "image_processing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <locale.h>
#include <sys/stat.h>
#include <time.h>

#define NUM_THREADS 18

void formatNumberWithCommas(const char *numStr, char *buffer);

void guardar_acumulados(unsigned long long lecturas, unsigned long long escrituras, double instrucciones);
void leer_acumulados(unsigned long long *lecturas, unsigned long long *escrituras, double *instrucciones);
void guardar_tiempo(double segundos);
double leer_tiempo();

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS);

    if (argc == 2 && strcmp(argv[1], "--start") == 0) {
        FILE *f = fopen("acumulador.txt", "w");
        if (f) {
            fprintf(f, "0 0 0.0\n");
            fclose(f);
        }
        FILE *t = fopen("tiempo.txt", "w");
        if (t) {
            fprintf(t, "0.0\n");
            fclose(t);
        }
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--end") == 0) {
        FILE *log = fopen("log-temp.txt", "w");
        if (!log) return 1;

        unsigned long long lecturas = 0, escrituras = 0;
        double instrucciones = 0;
        leer_acumulados(&lecturas, &escrituras, &instrucciones);
        double tiempoTotal = leer_tiempo();
        double mips = (instrucciones / 1e6) / tiempoTotal;

        char bufLect[64], bufEscr[64], bufMips[64];
        sprintf(bufLect, "%llu", lecturas);
        sprintf(bufEscr, "%llu", escrituras);
        sprintf(bufMips, "%.2f", mips);

        char outLect[64], outEscr[64], outMips[64];
        formatNumberWithCommas(bufLect, outLect);
        formatNumberWithCommas(bufEscr, outEscr);
        formatNumberWithCommas(bufMips, outMips);

        fprintf(log,
            "--- Reporte Final ---\n"
            "Kernel utilizado para desenfoque: 55\n"
            "Total de localidades leídas: %s\n"
            "Total de localidades escritas: %s\n"
            "Tiempo de ejecución: %.2f segundos\n"
            "MIPS estimados: %s\n",
            outLect, outEscr, tiempoTotal, outMips
        );

        fclose(log);
        return 0;
    }

    if (argc < 5) {
        fprintf(stderr, "Uso: %s <kernel> <inputDir> <outputDir> <imageFile>\n", argv[0]);
        return 1;
    }

    int kernelSize = atoi(argv[1]);
    const char *inputDir = argv[2];
    const char *outputDir = argv[3];
    const char *imageFile = argv[4];

    char entrada[512];
    snprintf(entrada, sizeof(entrada), "%s/%s", inputDir, imageFile);

    char nombreBase[128];
    strncpy(nombreBase, imageFile, sizeof(nombreBase));
    char *ext = strrchr(nombreBase, '.');
    if (ext) *ext = '\0';

    char salidaHorizontalGrises[512], salidaHorizontalColor[512];
    char salidaVerticalGrises[512], salidaVerticalColor[512];
    char salidaDesenfoque[512], salidaGrises[512];

    snprintf(salidaHorizontalGrises, sizeof(salidaHorizontalGrises), "%s/%s_hg.bmp", outputDir, nombreBase);
    snprintf(salidaHorizontalColor, sizeof(salidaHorizontalColor), "%s/%s_hc.bmp", outputDir, nombreBase);
    snprintf(salidaVerticalGrises, sizeof(salidaVerticalGrises), "%s/%s_vg.bmp", outputDir, nombreBase);
    snprintf(salidaVerticalColor, sizeof(salidaVerticalColor), "%s/%s_vc.bmp", outputDir, nombreBase);
    snprintf(salidaDesenfoque, sizeof(salidaDesenfoque), "%s/%s_blur_k%d.bmp", outputDir, nombreBase, kernelSize);
    snprintf(salidaGrises, sizeof(salidaGrises), "%s/%s_gris.bmp", outputDir, nombreBase);

    clock_t start = clock();

    unsigned long l1 = 0, e1 = 0, l2 = 0, e2 = 0, l3 = 0, e3 = 0;
    unsigned long l4 = 0, e4 = 0, l5 = 0, e5 = 0, l6 = 0, e6 = 0;

    invertirHorizontalGrises(entrada, salidaHorizontalGrises, NULL, &l1, &e1);
    invertirHorizontalColor(entrada, salidaHorizontalColor, NULL, &l2, &e2);
    invertirVerticalGrises(entrada, salidaVerticalGrises, NULL, &l3, &e3);
    invertirVerticalColor(entrada, salidaVerticalColor, NULL, &l4, &e4);
    aplicarDesenfoqueIntegral(entrada, salidaDesenfoque, kernelSize, NULL, &l5, &e5);
    convertirAGrises(entrada, salidaGrises, NULL, &l6, &e6);

    clock_t end = clock();
    double tiempo = (double)(end - start) / CLOCKS_PER_SEC;

    unsigned long long lecturas = l1 + l2 + l3 + l4 + (l5 * kernelSize * kernelSize) + l6;
    unsigned long long escrituras = e1 + e2 + e3 + e4 + e5 + e6;
    double instrucciones = (double)(lecturas + escrituras) * 20.0 * NUM_THREADS;

    guardar_acumulados(lecturas, escrituras, instrucciones);
    guardar_tiempo(tiempo);

    return 0;
}

// ---------- Funciones auxiliares ----------

void formatNumberWithCommas(const char *numStr, char *buffer) {
    char intPart[128], decPart[64] = "", temp[192];
    const char *dot = strchr(numStr, '.');
    if (dot) {
        size_t len = dot - numStr;
        strncpy(intPart, numStr, len); intPart[len] = '\0';
        strncpy(decPart, dot + 1, sizeof(decPart) - 1);
    } else strcpy(intPart, numStr);

    int len = strlen(intPart), commas = (len - 1) / 3, newLen = len + commas;
    temp[newLen] = '\0';

    for (int i = len - 1, j = newLen - 1, c = 0; i >= 0; --i, --j) {
        temp[j] = intPart[i];
        if (++c == 3 && i > 0) temp[--j] = ',', c = 0;
    }

    if (*decPart) sprintf(buffer, "%s.%s", temp, decPart);
    else strcpy(buffer, temp);
}

void guardar_acumulados(unsigned long long l, unsigned long long e, double ins) {
    unsigned long long lOld = 0, eOld = 0; double insOld = 0;
    leer_acumulados(&lOld, &eOld, &insOld);
    FILE *f = fopen("acumulador.txt", "w");
    if (f) {
        fprintf(f, "%llu %llu %.2f\n", lOld + l, eOld + e, insOld + ins);
        fclose(f);
    }
}

void leer_acumulados(unsigned long long *l, unsigned long long *e, double *ins) {
    FILE *f = fopen("acumulador.txt", "r");
    if (f) {
        fscanf(f, "%llu %llu %lf", l, e, ins);
        fclose(f);
    }
}

void guardar_tiempo(double segundos) {
    double viejo = leer_tiempo();
    FILE *f = fopen("tiempo.txt", "w");
    if (f) {
        fprintf(f, "%.2f\n", viejo + segundos);
        fclose(f);
    }
}

double leer_tiempo() {
    double t = 0.0;
    FILE *f = fopen("tiempo.txt", "r");
    if (f) {
        fscanf(f, "%lf", &t);
        fclose(f);
    }
    return t;
}
