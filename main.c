#include "image_processing.h"
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#define NUM_IMAGENES 5

int main(int argc, char **argv) {
    setlocale(LC_NUMERIC, "C");

    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int kernelSize;

    // Proceso 0 solicita el tamaño del kernel
    if (world_rank == 0) {
        do {
            printf("Ingrese el tamaño del kernel (impar entre 3 y 155): ");
            scanf("%d", &kernelSize);
        } while (kernelSize < 3 || kernelSize > 155 || kernelSize % 2 == 0);
    }

    // Se propaga a todos los procesos
    MPI_Bcast(&kernelSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

    unsigned long long totalLecturas = 0;
    unsigned long long totalEscrituras = 0;

    double start = MPI_Wtime();

    // Cada proceso maneja diferentes imágenes de forma distribuida
    for (int i = 1 + world_rank; i <= NUM_IMAGENES; i += world_size) {
        char inputFile[128], outputFile[128];

        sprintf(inputFile, "./images/image%d.bmp", i);

        sprintf(outputFile, "./processed/image%d_h_gris.bmp", i);
        invertirHorizontalGrises(inputFile, outputFile, NULL, &totalLecturas, &totalEscrituras);

        sprintf(outputFile, "./processed/image%d_h_color.bmp", i);
        invertirHorizontalColor(inputFile, outputFile, NULL, &totalLecturas, &totalEscrituras);

        sprintf(outputFile, "./processed/image%d_v_gris.bmp", i);
        invertirVerticalGrises(inputFile, outputFile, NULL, &totalLecturas, &totalEscrituras);

        sprintf(outputFile, "./processed/image%d_v_color.bmp", i);
        invertirVerticalColor(inputFile, outputFile, NULL, &totalLecturas, &totalEscrituras);

        sprintf(outputFile, "./processed/image%d_blur_k%d.bmp", i, kernelSize);
        aplicarBlur(inputFile, outputFile, kernelSize, NULL, &totalLecturas, &totalEscrituras);

        sprintf(outputFile, "./processed/image%d_gris.bmp", i);
        convertirAGris(inputFile, outputFile, NULL, &totalLecturas, &totalEscrituras);

        printf("Proceso %d procesó imagen %d\n", world_rank, i);
    }

    double end = MPI_Wtime();
    double localTime = end - start;

    // Variables para resultados globales
    unsigned long long globalLecturas = 0, globalEscrituras = 0;
    double maxTime = 0;

    // Reducción de resultados
    MPI_Reduce(&totalLecturas, &globalLecturas, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalEscrituras, &globalEscrituras, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&localTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        printf("\n======= Reporte final (MPI Distribuido) =======\n");
        printf("Imágenes procesadas: %d\n", NUM_IMAGENES);
        printf("Total de lecturas: %llu\n", globalLecturas);
        printf("Total de escrituras: %llu\n", globalEscrituras);
        printf("Tiempo total (máx): %.3f segundos\n", maxTime);
        printf("Procesos (computadoras): %d\n", world_size);
    }

    MPI_Finalize();
    return 0;
}
