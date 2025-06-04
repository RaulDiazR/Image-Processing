// main.c
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
#include <unistd.h>

#define NUM_THREADS 6 // Per process/thread
#define MAX_HOSTNAME 256

void formatNumberWithCommas(const char *numStr, char *buffer);
long long get_free_space_bytes(const char *path);
long long calcular_promedio_tamano_imagenes(const char *directorio, int *num_imagenes);

int main(int argc, char *argv[]) {
    // --------- Argument parsing ---------
    int num_imagenes_total = 600;           // default total images
    char *imagesDir      = "./images_test";    // default input folder
    char *outputDir      = "./processed_test"; // default output folder
    int  kernelSize      = 155;            // default kernel size

    if (argc >= 4) {
        kernelSize         = atoi(argv[1]); // odd between 3 and 155
        imagesDir          = argv[2];       // input directory
        outputDir          = argv[3];       // output directory
    }
    if (argc >= 5) {
        num_imagenes_total = atoi(argv[4]); // optional override total count
    }

    // Initialize MPI and threading
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS);

    // Open log file for all ranks
    FILE *log = fopen("log_temp.txt", "w");
    if (!log) {
        if (rank == 0) fprintf(stderr, "No se pudo abrir log_temp.txt\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Split by hostname for local gathers
    char hostname[MAX_HOSTNAME];
    gethostname(hostname, MAX_HOSTNAME);
    unsigned long host_hash = 5381;
    for (int c = 0; hostname[c] != '\0'; c++)
        host_hash = ((host_hash << 5) + host_hash) + hostname[c];
    MPI_Comm node_comm;
    MPI_Comm_split(MPI_COMM_WORLD, host_hash, rank, &node_comm);
    int node_rank;
    MPI_Comm_rank(node_comm, &node_rank);

    // --------- Compute average size ---------
    long long promedioTamano = 0;
    int numImagenesReal = 0;
    if (rank == 0) {
        promedioTamano = calcular_promedio_tamano_imagenes(imagesDir, &numImagenesReal);
        if (promedioTamano <= 0 || numImagenesReal == 0) {
            fprintf(stderr, "No se pudo calcular el tamaño promedio.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    MPI_Bcast(&promedioTamano, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    // --------- Broadcast kernel size ---------
    MPI_Bcast(&kernelSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // --------- Divide work ---------
    int imagesPerProc = num_imagenes_total / size;
    int start = rank * imagesPerProc + 1;
    int end   = (rank == size - 1) ? num_imagenes_total : start + imagesPerProc - 1;

    // Create output directory
    mkdir(outputDir, 0777);

    // --------- Check available space ---------
    long long estimacionPorImagen = promedioTamano * 6;
    long long espacioLocal = 0;
    if (node_rank == 0) {
        espacioLocal = get_free_space_bytes(outputDir);
    }
    long long espacioNodo = espacioLocal;
    long long *espaciosPorNodo = NULL;
    if (rank == 0) {
        espaciosPorNodo = calloc(size, sizeof(long long));
    }
    MPI_Gather(&espacioNodo, 1, MPI_LONG_LONG, espaciosPorNodo, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        long long espacioTotal = 0;
        for (int i = 0; i < size; i++) espacioTotal += espaciosPorNodo[i];
        long long espacioNecesario = estimacionPorImagen * num_imagenes_total * 1.08;
        if (espacioTotal < espacioNecesario) {
            fprintf(stderr, "ERROR: Espacio insuficiente en el clúster. Requiere %.2f GB, disponible %.2f GB\n",
                    (double)espacioNecesario / (1024.0 * 1024.0 * 1024.0),
                    (double)espacioTotal   / (1024.0 * 1024.0 * 1024.0));
            free(espaciosPorNodo);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        free(espaciosPorNodo);
    }

    // --------- Processing loop ---------
    unsigned long long totalLecturas     = 0;
    unsigned long long totalLecturasBlur = 0;
    unsigned long long totalEscrituras   = 0;
    double startTime = MPI_Wtime();

    int localIndex = 1;
    for (int i = start; i <= end; i++) {
        char entrada[512], salida1[512], salida2[512], salida3[512];
        char salida4[512], salida5[512], salida6[512];
        snprintf(entrada, sizeof(entrada), "%s/img%d.bmp", imagesDir, i);
        snprintf(salida1, sizeof(salida1), "%s/img%d_hg.bmp", outputDir, i);
        snprintf(salida2, sizeof(salida2), "%s/img%d_hc.bmp", outputDir, i);
        snprintf(salida3, sizeof(salida3), "%s/img%d_vg.bmp", outputDir, i);
        snprintf(salida4, sizeof(salida4), "%s/img%d_vc.bmp", outputDir, i);
        snprintf(salida5, sizeof(salida5), "%s/img%d_blur_k%d.bmp", outputDir, i, kernelSize);
        snprintf(salida6, sizeof(salida6), "%s/img%d_gris.bmp", outputDir, i);

        unsigned long lecturas = 0, escrituras = 0, lecturasBlur = 0;
        invertirHorizontalGrises(entrada, salida1, log, &lecturas, &escrituras);
        invertirHorizontalColor(entrada, salida2, log, &lecturas, &escrituras);
        invertirVerticalGrises(entrada, salida3, log, &lecturas, &escrituras);
        invertirVerticalColor(entrada, salida4, log, &lecturas, &escrituras);
        aplicarDesenfoqueIntegral(entrada, salida5, kernelSize, log, &lecturasBlur, &escrituras);
        convertirAGrises(entrada, salida6, log, &lecturas, &escrituras);

        totalLecturas     += lecturas;
        totalLecturasBlur += (unsigned long long)lecturasBlur * kernelSize * kernelSize;
        totalEscrituras   += escrituras;

        fprintf(stderr, "Procesador %d en %s: procesando imagen %d/%d\n", 
        rank, hostname, i, num_imagenes_total);
        fflush(stdout);
    }
    printf("\n");

    // --------- Aggregation & final report ---------
    double endTime = MPI_Wtime();
    double localTime = endTime - startTime;
    unsigned long long globalLecturas = 0, globalBlur = 0, globalEscrituras = 0;
    MPI_Reduce(&totalLecturasBlur, &globalBlur,      1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalLecturas,     &globalLecturas,1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalEscrituras,   &globalEscrituras,1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    double maxTime;
    MPI_Reduce(&localTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        double totalTime = maxTime;
        unsigned long long totalAccesos = globalLecturas + globalEscrituras + globalBlur;
        double instrucciones = (double)totalAccesos * 20.0 * NUM_THREADS;
        double mips = (instrucciones / 1e6) / totalTime;
        double mbps = (double)totalAccesos / (1024.0 * 1024.0 * 1024.0) / totalTime;

        char mipsStr[64], formattedMips[64];
        sprintf(mipsStr, "%.2f", mips);
        formatNumberWithCommas(mipsStr, formattedMips);
        char mbpsStr[64], formattedMbps[64];
        sprintf(mbpsStr, "%.2f", mbps);
        formatNumberWithCommas(mbpsStr, formattedMbps);

        char lecStr[64], escStr[64], formattedLecturas[64], formattedEscrituras[64];
        sprintf(lecStr, "%llu", globalLecturas + globalBlur);
        sprintf(escStr, "%llu", globalEscrituras);
        formatNumberWithCommas(lecStr, formattedLecturas);
        formatNumberWithCommas(escStr, formattedEscrituras);

        int minutes = (int)totalTime / 60;
        double seconds = totalTime - (minutes * 60);

        FILE *finalLog = fopen("final_log.txt", "w");
        if (finalLog) {
            fprintf(finalLog, "--- Reporte Final ---\n");
            fprintf(finalLog, "Kernel utilizado: %d\n", kernelSize);
            fprintf(finalLog, "Total lecturas: %s\n", formattedLecturas);
            fprintf(finalLog, "Total escrituras: %s\n", formattedEscrituras);
            fprintf(finalLog, "Tiempo total: %d min %.2f s\n", minutes, seconds);
            fprintf(finalLog, "Velocidad: %s GB/s\n", formattedMbps);
            fprintf(finalLog, "MIPS estimados: %s\n", formattedMips);
            fclose(finalLog);
            // printf("\nResumen escrito en final_log.txt\n");
        } else {
            fprintf(stderr, "No se pudo escribir final_log.txt\n");
        }
    }

    fclose(log);
    MPI_Finalize();
    return 0;
}

// Definitions for helper functions (unchanged)
long long get_free_space_bytes(const char *path) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) return -1;
    return (long long)stat.f_bsize * stat.f_bavail;
}

long long calcular_promedio_tamano_imagenes(const char *directorio, int *num_imagenes) {
    DIR *dir = opendir(directorio);
    if (!dir) return -1;
    struct dirent *entry;
    long long total_size = 0;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".bmp")) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", directorio, entry->d_name);
            struct stat st;
            if (stat(path, &st) == 0) {
                total_size += st.st_size;
                count++;
            }
        }
    }
    closedir(dir);
    if (num_imagenes) *num_imagenes = count;
    return count > 0 ? total_size / count : -1;
}

void formatNumberWithCommas(const char *numStr, char *buffer) {
    char intPart[64], decPart[64] = "";
    char temp[64];
    const char *dot = strchr(numStr, '.');
    if (dot) {
        size_t intLen = dot - numStr;
        strncpy(intPart, numStr, intLen);
        intPart[intLen] = '\0';
        strncpy(decPart, dot + 1, sizeof(decPart)-1);
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
    if (decPart[0]) sprintf(buffer, "%s.%s", temp, decPart);
    else strcpy(buffer, temp);
}
