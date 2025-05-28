#include "image_processing.h"
#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define NUM_THREADS 6

void formatNumberWithCommas(const char *numStr, char *buffer);
void guardar_acumulados(unsigned long long lecturas, unsigned long long escrituras, double instrucciones);
void leer_acumulados(unsigned long long *lecturas, unsigned long long *escrituras, double *instrucciones);
void guardar_tiempo(double segundos);
double leer_tiempo();

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS);

    if (argc == 2 && strcmp(argv[1], "--start") == 0) {
        if (rank == 0) {
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
        }
        MPI_Finalize();
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--end") == 0) {
        if (rank == 0) {
            FILE *log = fopen("log-temp.txt", "w");
            if (!log) {
                MPI_Finalize();
                return 1;
            }

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
        }

        MPI_Finalize();
        return 0;
    }

    if (argc < 5) {
        if (rank == 0)
            fprintf(stderr, "Uso: %s <kernel> <inputDir> <outputDir> <imageFile>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int kernel = atoi(argv[1]);
    const char *inputDir = argv[2];
    const char *outputDir = argv[3];
    const char *imageName = argv[4];

    char entrada[512];
    snprintf(entrada, sizeof(entrada), "%s/%s", inputDir, imageName);

    char base[128];
    strncpy(base, imageName, sizeof(base));
    char *ext = strrchr(base, '.');
    if (ext) *ext = '\0';

    char salida1[512], salida2[512], salida3[512], salida4[512], salida5[512], salida6[512];
    snprintf(salida1, sizeof(salida1), "%s/%s_hg.bmp", outputDir, base);
    snprintf(salida2, sizeof(salida2), "%s/%s_hc.bmp", outputDir, base);
    snprintf(salida3, sizeof(salida3), "%s/%s_vg.bmp", outputDir, base);
    snprintf(salida4, sizeof(salida4), "%s/%s_vc.bmp", outputDir, base);
    snprintf(salida5, sizeof(salida5), "%s/%s_blur_k%d.bmp", outputDir, base, kernel);
    snprintf(salida6, sizeof(salida6), "%s/%s_gris.bmp", outputDir, base);

    unsigned long lecturasLocal = 0, lecturasBlurLocal = 0, escriturasLocal = 0;
    double startTime = MPI_Wtime();

    // Distribuir los efectos entre procesos
    if (rank == 0) invertirHorizontalGrises(entrada, salida1, NULL, &lecturasLocal, &escriturasLocal);
    else if (rank == 1) invertirHorizontalColor(entrada, salida2, NULL, &lecturasLocal, &escriturasLocal);
    else if (rank == 2) invertirVerticalGrises(entrada, salida3, NULL, &lecturasLocal, &escriturasLocal);
    else if (rank == 3) invertirVerticalColor(entrada, salida4, NULL, &lecturasLocal, &escriturasLocal);
    else if (rank == 4) aplicarDesenfoqueIntegral(entrada, salida5, kernel, NULL, &lecturasBlurLocal, &escriturasLocal);
    else if (rank == 5) convertirAGrises(entrada, salida6, NULL, &lecturasLocal, &escriturasLocal);

    double elapsed = MPI_Wtime() - startTime;

    unsigned long long globalLecturas = 0, globalLecturasBlur = 0, globalEscrituras = 0;
    MPI_Reduce(&lecturasLocal, &globalLecturas, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&lecturasBlurLocal, &globalLecturasBlur, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&escriturasLocal, &globalEscrituras, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    double maxTime = 0;
    MPI_Reduce(&elapsed, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        unsigned long long totalLecturas = globalLecturas + globalLecturasBlur * kernel * kernel;
        double instrucciones = (double)(totalLecturas + globalEscrituras) * 20.0 * NUM_THREADS;

        guardar_acumulados(totalLecturas, globalEscrituras, instrucciones);
        guardar_tiempo(maxTime);
    }

    MPI_Finalize();
    return 0;
}

// Utilidades
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
