// main.c (MPI version)
#include "image_processing.h"
#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>

#define NUM_IMAGENES 10
#define NUM_THREADS 5 // Per process/thread

void formatNumberWithCommas(const char *numStr, char *buffer);

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // This process's ID
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Total processes

    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS);

    int kernelSize;

    if (rank == 0) {
        char input[16];
        do {
            printf("Ingrese el tamaño del kernel (impar entre 3 y 155): ");
            fflush(stdout);  // Make sure it prints before blocking
            if (!fgets(input, sizeof(input), stdin)) {
                fprintf(stderr, "Error reading input\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            kernelSize = atoi(input);
        } while (kernelSize < 3 || kernelSize > 155 || kernelSize % 2 == 0);
    }

    // Broadcast kernel size to all processes
    MPI_Bcast(&kernelSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Open individual log file per rank
    char logFilename[64];
    snprintf(logFilename, sizeof(logFilename), "./individual_logs/log_rank%d.txt", rank);
    FILE *log = fopen(logFilename, "w");
    if (!log) {
        perror("No se pudo abrir el archivo de log");
        MPI_Finalize();
        return 1;
    }

    // Calculate image range for each process
    int imagesPerProc = NUM_IMAGENES / size;
    int start = rank * imagesPerProc + 1;
    int end = (rank == size - 1) ? NUM_IMAGENES : start + imagesPerProc - 1;

    unsigned long long totalLecturas = 0;
    unsigned long long totalEscrituras = 0;
    int processedImgs = 0;
    clock_t startTime = clock();

    for (int i = start; i <= end; i++) {
        char entrada[128], salidaHorizontalGrises[128], salidaHorizontalColor[128];
        char salidaVerticalGrises[128], salidaVerticalColor[128], salidaDesenfoque[128], salidaGrises[128];

        snprintf(entrada, sizeof(entrada), "./images/image%d.bmp", i);
        snprintf(salidaHorizontalGrises, sizeof(salidaHorizontalGrises), "./processed/image%d_horizontal_gris.bmp", i);
        snprintf(salidaHorizontalColor, sizeof(salidaHorizontalColor), "./processed/image%d_horizontal_color.bmp", i);
        snprintf(salidaVerticalGrises, sizeof(salidaVerticalGrises), "./processed/image%d_vertical_gris.bmp", i);
        snprintf(salidaVerticalColor, sizeof(salidaVerticalColor), "./processed/image%d_vertical_color.bmp", i);
        snprintf(salidaDesenfoque, sizeof(salidaDesenfoque), "./processed/image%d_blur_kernelSize%d.bmp", i, kernelSize);
        snprintf(salidaGrises, sizeof(salidaGrises), "./processed/image%d_gris.bmp", i);

        unsigned long lecturas = 0, escrituras = 0, lecturasBlur = 0;

        invertirHorizontalGrises(entrada, salidaHorizontalGrises, log, &lecturas, &escrituras);
        invertirHorizontalColor(entrada, salidaHorizontalColor, log, &lecturas, &escrituras);
        invertirVerticalGrises(entrada, salidaVerticalGrises, log, &lecturas, &escrituras);
        invertirVerticalColor(entrada, salidaVerticalColor, log, &lecturas, &escrituras);
        aplicarDesenfoqueIntegral(entrada, salidaDesenfoque, kernelSize, log, &lecturasBlur, &escrituras);
        convertirAGrises(entrada, salidaGrises, log, &lecturas, &escrituras);

        totalLecturas += lecturas + (lecturasBlur * kernelSize * kernelSize);
        totalEscrituras += escrituras;

        processedImgs += 1;
        printf("\rRango %d procesando imagen %d/%d      ", rank, processedImgs, end - start + 1);
        fflush(stdout);
    }

    printf("\n");

    clock_t endTime = clock();
    double localTime = (double)(endTime - startTime) / CLOCKS_PER_SEC;

    unsigned long long totalAccesos = totalLecturas + totalEscrituras;
    double instrucciones = (long double)totalAccesos * 20.0 * NUM_THREADS;
    double mips = (instrucciones / 1e6) / localTime;

    char mipsStr[64], formattedMips[64];
    sprintf(mipsStr, "%.2f", mips);
    formatNumberWithCommas(mipsStr, formattedMips);

    fprintf(log, "\n--- Rango %d Finalizado ---\n", rank);
    fprintf(log, "Kernel: %d\n", kernelSize);
    fprintf(log, "Tiempo: %.2f segundos\n", localTime);
    fprintf(log, "Lecturas: %llu\n", totalLecturas);
    fprintf(log, "Escrituras: %llu\n", totalEscrituras);
    fprintf(log, "MIPS: %s\n", formattedMips);

    fclose(log);

    // Gather global totals on rank 0
    double maxTime;
    unsigned long long globalLecturas = 0, globalEscrituras = 0;
    MPI_Reduce(&totalLecturas, &globalLecturas, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalEscrituras, &globalEscrituras, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);


    MPI_Reduce(&localTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        double tiempoTotal = maxTime;
        unsigned long long totalAccesos = globalLecturas + globalEscrituras;
        double instrucciones = (double)totalAccesos * 20.0 * NUM_THREADS;
        double mips = (instrucciones / 1e6) / tiempoTotal;
        double mbps = (double)totalAccesos / (1024.0 * 1024.0) / tiempoTotal;

        char mipsStr[64], formattedMips[64];
        sprintf(mipsStr, "%.2f", mips);
        formatNumberWithCommas(mipsStr, formattedMips);

        char lecStr[64], escStr[64];
        sprintf(lecStr, "%llu", globalLecturas);
        sprintf(escStr, "%llu", globalEscrituras);

        char formattedLecturas[64], formattedEscrituras[64];
        formatNumberWithCommas(lecStr, formattedLecturas);
        formatNumberWithCommas(escStr, formattedEscrituras);

        int minutes = (int)tiempoTotal / 60;
        double seconds = tiempoTotal - (minutes * 60);

        FILE *finalLog = fopen("final_log.txt", "w");
        if (finalLog) {
            fprintf(finalLog, "--- Reporte Final ---\n");
            fprintf(finalLog, "Kernel utilizado para desenfoque: %d\n", kernelSize);
            fprintf(finalLog, "Total de localidades leídas: %s\n", formattedLecturas);
            fprintf(finalLog, "Total de localidades escritas: %s\n", formattedEscrituras);
            fprintf(finalLog, "Tiempo total de ejecución: %d minutos con %.2f segundos\n", minutes, seconds);
            fprintf(finalLog, "Velocidad de procesamiento: %.2f MB/s\n", mbps);
            fprintf(finalLog, "MIPS estimados: %s\n", formattedMips);
            fclose(finalLog);
            printf("\nResumen escrito en final_log.txt\n");
        } else {
            fprintf(stderr, "No se pudo escribir final_log.txt\n");
        }
    }

    MPI_Finalize();
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
        snprintf(buffer, 128, "%s.%s", temp, decPart);
    } else {
        strcpy(buffer, temp);
    }
}
