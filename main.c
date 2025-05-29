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
#include <sys/statvfs.h>
#include <unistd.h>

#define NUM_IMAGENES 600
#define NUM_THREADS 6 // Per process/thread
#define MAX_HOSTNAME 256

void formatNumberWithCommas(const char *numStr, char *buffer);

long long get_free_space_bytes(const char *path) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) return -1;
    return (long long)stat.f_bsize * (long long)stat.f_bavail;
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
    return (count > 0) ? total_size / count : -1;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    setlocale(LC_NUMERIC, "C");
    omp_set_num_threads(NUM_THREADS);

    char hostname[MAX_HOSTNAME];
    gethostname(hostname, MAX_HOSTNAME);

    unsigned long host_hash = 5381;
    for (int c = 0; hostname[c] != '\0'; c++)
        host_hash = ((host_hash << 5) + host_hash) + hostname[c]; // djb2

    MPI_Comm node_comm;
    MPI_Comm_split(MPI_COMM_WORLD, host_hash, rank, &node_comm);

    int node_rank;
    MPI_Comm_rank(node_comm, &node_rank);

    long long promedioTamano = 0;
    int numImagenesReal = 0;

    if (rank == 0) {
        promedioTamano = calcular_promedio_tamano_imagenes("./images", &numImagenesReal);
        if (promedioTamano <= 0 || numImagenesReal == 0) {
            fprintf(stderr, "No se pudo calcular el tamaño promedio de las imágenes.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    MPI_Bcast(&promedioTamano, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    int kernelSize;
    if (rank == 0) {
        char input[16];
        do {
            printf("Ingrese el tamaño del kernel (impar entre 3 y 155) [155 por defecto]: ");
            fflush(stdout);
            if (!fgets(input, sizeof(input), stdin)) MPI_Abort(MPI_COMM_WORLD, 1);
            if (input[0] == '\n') kernelSize = 155;
            else kernelSize = atoi(input);
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

    const char *outputDir = "./processed/";
    mkdir(outputDir, 0777);

    long long estimacionPorImagenProcesada = promedioTamano * 6;
    long long espacioDisponibleLocal = 0;
    if (node_rank == 0) {
        espacioDisponibleLocal = get_free_space_bytes(outputDir);
    }

    long long espacioNodo = espacioDisponibleLocal;
    long long *espaciosPorNodo = NULL;
    if (rank == 0) {
        espaciosPorNodo = calloc(size, sizeof(long long));
    }
    MPI_Gather(&espacioNodo, 1, MPI_LONG_LONG, espaciosPorNodo, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        long long espacioTotal = 0;
        for (int i = 0; i < size; i++) {
            espacioTotal += espaciosPorNodo[i];
        }
        long long espacioNecesario = estimacionPorImagenProcesada * NUM_IMAGENES * 1.08;
        if (espacioTotal < espacioNecesario) {
            fprintf(stderr, "ERROR: Espacio insuficiente en el clúster. Requiere %.2f GB, disponible %.2f GB\n",
                    (double)espacioNecesario / (1024 * 1024 * 1024),
                    (double)espacioTotal / (1024 * 1024 * 1024));
            free(espaciosPorNodo);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        free(espaciosPorNodo);
    }

    unsigned long long totalLecturas = 0, totalLecturasBlur = 0, totalEscrituras = 0;
    double startTime = MPI_Wtime();

    int local_image_index = 1;
    for (int i = start; i <= end; i++) {
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
        double now = MPI_Wtime() - startTime;
        printf("Rank %d en %s procesando imagen %d/%d en t=%.2f s\n", rank, hostname, local_image_index, imagesPerProc, now);
                
        local_image_index++;
        fflush(stdout);
    }
    printf("\n");

    double endTime = MPI_Wtime();
    double localTime = endTime - startTime;

    unsigned long long globalLecturas = 0, globalLecturasBlur = 0, globalEscrituras = 0;
    MPI_Reduce(&totalLecturasBlur, &globalLecturasBlur, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalLecturas, &globalLecturas, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalEscrituras, &globalEscrituras, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    double maxTime;
    MPI_Reduce(&localTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        double tiempoTotal = maxTime;
        unsigned long long totalAccesos = globalLecturas + globalEscrituras + globalLecturasBlur;
        double instrucciones = (double)totalAccesos * 20.0 * NUM_THREADS;
        double mips = (instrucciones / 1e6) / tiempoTotal;
        double mbps = (double)totalAccesos / (1024.0 * 1024.0 * 1024.0) / tiempoTotal;

        char mipsStr[64], formattedMips[64];
        sprintf(mipsStr, "%.2f", mips);
        formatNumberWithCommas(mipsStr, formattedMips);
        
        char mbpsStr[64], formattedMbps[64];
        sprintf(mbpsStr, "%.2f", mbps);
        formatNumberWithCommas(mbpsStr, formattedMbps);

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
            fprintf(finalLog, "Velocidad de procesamiento: %s GB/s\n", formattedMbps);
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
