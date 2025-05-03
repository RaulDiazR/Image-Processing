// image_processing.c
#include "image_processing.h"
#include <omp.h>

static void logError(FILE *log, const char *msg) {
    if (log) fprintf(log, "Error: %s\n", msg);
}

void invertirHorizontalGrises(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras) {
    FILE *fin = fopen(entrada, "rb");
    if (!fin) { logError(log, "No se pudo abrir la imagen de entrada."); return; }

    FILE *fout = fopen(salida, "wb");
    if (!fout) { fclose(fin); logError(log, "No se pudo crear la imagen de salida."); return; }

    BMPHeader header;
    fread(&header, sizeof(BMPHeader), 1, fin);
    *lecturas += sizeof(BMPHeader);
    fwrite(&header, sizeof(BMPHeader), 1, fout);
    *escrituras += sizeof(BMPHeader);

    DIBHeader dib;
    fread(&dib, sizeof(DIBHeader), 1, fin);
    *lecturas += sizeof(DIBHeader);
    fwrite(&dib, sizeof(DIBHeader), 1, fout);
    *escrituras += sizeof(DIBHeader);

    if (dib.bitsPerPixel != 24) {
        fclose(fin); fclose(fout);
        logError(log, "Solo se soportan imágenes de 24 bits.");
        return;
    }

    int padding = (4 - (dib.width * 3) % 4) % 4;
    size_t rowSize = dib.width * 3 + padding;
    size_t imageSize = rowSize * dib.height;
    
    uint8_t *buffer = malloc(imageSize);
    if (!buffer) {
        fclose(fin); fclose(fout);
        logError(log, "Memoria insuficiente.");
        return;
    }

    fread(buffer, 1, imageSize, fin);
    *lecturas += imageSize;
    fclose(fin);

    // Crear un buffer de salida
    uint8_t *output = malloc(imageSize);
    if (!output) {
        free(buffer);
        fclose(fout);
        logError(log, "Memoria insuficiente.");
        return;
    }

    #pragma omp parallel for
    for (int y = 0; y < dib.height; y++) {
        uint8_t *srcRow = buffer + y * rowSize;
        uint8_t *dstRow = output + y * rowSize;

        for (int x = 0; x < dib.width; x++) {
            int invX = dib.width - 1 - x;
            uint8_t r = srcRow[invX*3+2];
            uint8_t g = srcRow[invX*3+1];
            uint8_t b = srcRow[invX*3+0];
            uint8_t gray = (uint8_t)(0.21*r + 0.72*g + 0.07*b);

            dstRow[x*3+0] = gray;
            dstRow[x*3+1] = gray;
            dstRow[x*3+2] = gray;
        }
        for (int p = 0; p < padding; p++) {
            dstRow[dib.width*3 + p] = 0x00;
        }
    }

    fwrite(output, 1, imageSize, fout);
    *escrituras += imageSize;

    free(buffer);
    free(output);
    fclose(fout);
}

void invertirHorizontalColor(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras) {
    FILE *fin = fopen(entrada, "rb");
    if (!fin) { logError(log, "No se pudo abrir la imagen de entrada."); return; }

    FILE *fout = fopen(salida, "wb");
    if (!fout) { fclose(fin); logError(log, "No se pudo crear la imagen de salida."); return; }

    BMPHeader header;
    fread(&header, sizeof(BMPHeader), 1, fin);
    *lecturas += sizeof(BMPHeader);
    fwrite(&header, sizeof(BMPHeader), 1, fout);
    *escrituras += sizeof(BMPHeader);

    DIBHeader dib;
    fread(&dib, sizeof(DIBHeader), 1, fin);
    *lecturas += sizeof(DIBHeader);
    fwrite(&dib, sizeof(DIBHeader), 1, fout);
    *escrituras += sizeof(DIBHeader);

    if (dib.bitsPerPixel != 24) {
        fclose(fin); fclose(fout);
        logError(log, "Solo se soportan imágenes de 24 bits.");
        return;
    }

    int padding = (4 - (dib.width * 3) % 4) % 4;
    size_t rowSize = dib.width * 3 + padding;
    size_t imageSize = rowSize * dib.height;
    
    uint8_t *buffer = malloc(imageSize);
    if (!buffer) {
        fclose(fin); fclose(fout);
        logError(log, "Memoria insuficiente.");
        return;
    }

    fread(buffer, 1, imageSize, fin);
    *lecturas += imageSize;
    fclose(fin);

    // Crear un buffer de salida
    uint8_t *output = malloc(imageSize);
    if (!output) {
        free(buffer);
        fclose(fout);
        logError(log, "Memoria insuficiente.");
        return;
    }

    #pragma omp parallel for
    for (int y = 0; y < dib.height; y++) {
        uint8_t *srcRow = buffer + y * rowSize;
        uint8_t *dstRow = output + y * rowSize;
    
        for (int x = 0; x < dib.width; x++) {
            int invX = dib.width - 1 - x;
            dstRow[x*3+0] = srcRow[invX*3+0];
            dstRow[x*3+1] = srcRow[invX*3+1];
            dstRow[x*3+2] = srcRow[invX*3+2];
        }
        for (int p = 0; p < padding; p++) {
            dstRow[dib.width*3 + p] = 0x00;
        }
    }

    fwrite(output, 1, imageSize, fout);
    *escrituras += imageSize;

    free(buffer);
    free(output);
    fclose(fout);
}
