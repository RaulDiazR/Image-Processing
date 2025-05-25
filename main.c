// main.c (MPI version con almacenamiento distribuido local y chequeo de espacio)
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
#include <sys/statvfs.h>

#define NUM_IMAGENES 100
#define NUM_THREADS 5 // Per process/thread

void formatNumberWithCommas(const char *numStr, char *buffer);

long long get_free_space_bytes(const char *path) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) return -1;
    return (long long)stat.f_bsize * (long long)stat.f_bavail;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS);

    int kernelSize;
    if (rank == 0) {
        char input[16];
        do {
            printf("Ingrese el tamaño del kernel (impar entre 3 y 155): ");
            fflush(stdout);
            if (!fgets(input, sizeof(input), stdin)) MPI_Abort(MPI_COMM_WORLD, 1);
            kernelSize = atoi(input);
        } while (kernelSize < 3 || kernelSize > 155 || kernelSize % 2 == 0);
    }
    MPI_Bcast(&kernelSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

    char logFilename[64];
    snprintf(logFilename, sizeof(logFilename), "./individual_logs/log_rank%d.txt", rank);
    FILE *log = fopen(logFilename, "w");
    if (!log) {
        perror("No se pudo abrir el archivo de log");
        MPI_Finalize();
        return 1;
    }

    int imagesPerProc = NUM_IMAGENES / size;
    int start = rank * imagesPerProc + 1;
    int end = (rank == size - 1) ? NUM_IMAGENES : start + imagesPerProc - 1;

    const char *outputDir = "./processed_local/";
    mkdir(outputDir, 0777);

    long long espacioDisponible = get_free_space_bytes(outputDir);
    long long espacioEstimadoPorImagen = 2.5 * 1024 * 1024;
    int maxImagenesPosibles = espacioDisponible > 0 ? espacioDisponible / espacioEstimadoPorImagen : 0;

    unsigned long long totalLecturas = 0, totalLecturasBlur = 0, totalEscrituras = 0;
    int processedImgs = 0;
    clock_t startTime = clock();

    for (int i = start; i <= end && processedImgs < maxImagenesPosibles; i++) {
        char entrada[128], salida1[128], salida2[128], salida3[128], salida4[128], salida5[128], salida6[128];
        snprintf(entrada, sizeof(entrada), "./images/image%d.bmp", i);
        snprintf(salida1, sizeof(salida1), "%s/image%d_horizontal_gris.bmp", outputDir, i);
        snprintf(salida2, sizeof(salida2), "%s/image%d_horizontal_color.bmp", outputDir, i);
        snprintf(salida3, sizeof(salida3), "%s/image%d_vertical_gris.bmp", outputDir, i);
        snprintf(salida4, sizeof(salida4), "%s/image%d_vertical_color.bmp", outputDir, i);
        snprintf(salida5, sizeof(salida5), "%s/image%d_blur_kernelSize%d.bmp", outputDir, i, kernelSize);
        snprintf(salida6, sizeof(salida6), "%s/image%d_gris.bmp", outputDir, i);

        unsigned long lecturas = 0, escrituras = 0, lecturasBlur = 0;

        invertirHorizontalGrises(entrada, salida1, log, &lecturas, &escrituras);
        invertirHorizontalColor(entrada, salida2, log, &lecturas, &escrituras);
        invertirVerticalGrises(entrada, salida3, log, &lecturas, &escrituras);
        invertirVerticalColor(entrada, salida4, log, &lecturas, &escrituras);
        aplicarDesenfoqueIntegral(entrada, salida5, kernelSize, log, &lecturasBlur, &escrituras);
        convertirAGrises(entrada, salida6, log, &lecturas, &escrituras);

        totalLecturas += lecturas;
        totalLecturasBlur += lecturasBlur * kernelSize * kernelSize;
        totalEscrituras += escrituras;
        processedImgs++;
        printf("\rRango %d procesando imagen %d/%d      ", rank, processedImgs, end - start + 1);
        fflush(stdout);
    }
    printf("\n");

    clock_t endTime = clock();
    double localTime = (double)(endTime - startTime) / CLOCKS_PER_SEC;

    long long espacioUtilizado = processedImgs * espacioEstimadoPorImagen;
    long long totalEspacioUtilizado;
    MPI_Reduce(&espacioUtilizado, &totalEspacioUtilizado, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    unsigned long long globalLecturas = 0, globalLecturasBlur = 0, globalEscrituras = 0;
    MPI_Reduce(&totalLecturasBlur, &globalLecturasBlur, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalLecturas, &globalLecturas, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalEscrituras, &globalEscrituras, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    double maxTime;
    MPI_Reduce(&localTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        long long espacioTotalEstimado = NUM_IMAGENES * espacioEstimadoPorImagen;
        long long espacioDisponibleMaster = get_free_space_bytes("./processed_local/");
        if (totalEspacioUtilizado > espacioTotalEstimado || espacioDisponibleMaster < espacioEstimadoPorImagen) {
            fprintf(stderr, "\nADVERTENCIA: Podría no haber suficiente espacio entre todos los nodos para guardar las imagenes procesadas.\n");
        }
        double tiempoTotal = maxTime;
        unsigned long long totalAccesos = globalLecturas + globalEscrituras + globalLecturasBlur;
        double instrucciones = (double)totalAccesos * 20.0 * NUM_THREADS;
        double mips = (instrucciones / 1e6) / tiempoTotal;
        double mbps = (double)totalAccesos / (1024.0 * 1024.0) / tiempoTotal;

        char mipsStr[64], formattedMips[64];
        sprintf(mipsStr, "%.2f", mips);
        formatNumberWithCommas(mipsStr, formattedMips);

        char lecStr[64], escStr[64];
        sprintf(lecStr, "%llu", globalLecturas + globalLecturasBlur);
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
            fprintf(finalLog, "Velocidad de procesamiento: %.2f MB/s\n", formattedMips);
            fprintf(finalLog, "MIPS estimados: %s\n", formattedMips);
            fclose(finalLog);
            printf("\nResumen escrito en final_log.txt\n");
        } else {
            fprintf(stderr, "No se pudo escribir final_log.txt\n");
        }
    }

    fclose(log);
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
    int i = len - 1, j = newLen - 1, count = 0;
    while (i >= 0) {
        temp[j--] = intPart[i--];
        if (++count == 3 && i >= 0) {
            temp[j--] = ',';
            count = 0;
        }
    }
    if (strlen(decPart) > 0) sprintf(buffer, "%s.%s", temp, decPart);
    else strcpy(buffer, temp);
}
