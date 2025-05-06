// image_processing.c
#include "image_processing.h"
#include <omp.h>

static void logError(FILE *log, const char *msg) {
    if (log) fprintf(log, "Error: %s\n", msg);
}
void convertirAGrises(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras) {
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
            uint8_t r = srcRow[x*3+2];
            uint8_t g = srcRow[x*3+1];
            uint8_t b = srcRow[x*3+0];
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

void aplicarDesenfoqueIntegral(const char *entrada, const char *salida, int kernelSize, FILE *log, unsigned long *lecturas, unsigned long *escrituras) {
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

    int w = dib.width, h = dib.height;
    int padding = (4 - (w * 3) % 4) % 4;
    size_t rowSize = w * 3 + padding;
    size_t imageSize = rowSize * h;

    uint8_t *image = malloc(imageSize);
    if (!image) { logError(log, "Memoria insuficiente."); return; }
    fread(image, 1, imageSize, fin);
    *lecturas += imageSize;
    fclose(fin);

    uint32_t **sumR = malloc(h * sizeof(uint32_t *));
    uint32_t **sumG = malloc(h * sizeof(uint32_t *));
    uint32_t **sumB = malloc(h * sizeof(uint32_t *));
    for (int i = 0; i < h; i++) {
        sumR[i] = calloc(w, sizeof(uint32_t));
        sumG[i] = calloc(w, sizeof(uint32_t));
        sumB[i] = calloc(w, sizeof(uint32_t));
    }

    // Build integral images
    for (int y = 0; y < h; y++) {
        uint8_t *row = image + y * rowSize;
        for (int x = 0; x < w; x++) {
            uint8_t *px = row + x * 3;
            int b = px[0], g = px[1], r = px[2];

            sumR[y][x] = r + (x > 0 ? sumR[y][x-1] : 0) + (y > 0 ? sumR[y-1][x] : 0) - (x > 0 && y > 0 ? sumR[y-1][x-1] : 0);
            sumG[y][x] = g + (x > 0 ? sumG[y][x-1] : 0) + (y > 0 ? sumG[y-1][x] : 0) - (x > 0 && y > 0 ? sumG[y-1][x-1] : 0);
            sumB[y][x] = b + (x > 0 ? sumB[y][x-1] : 0) + (y > 0 ? sumB[y-1][x] : 0) - (x > 0 && y > 0 ? sumB[y-1][x-1] : 0);
        }
    }

    uint8_t *output = malloc(imageSize);
    if (!output) { logError(log, "Memoria insuficiente."); return; }

    int r = kernelSize / 2;

    #pragma omp parallel for
    for (int y = 0; y < h; y++) {
        uint8_t *outRow = output + y * rowSize;

        for (int x = 0; x < w; x++) {
            int x1 = (x - r < 0) ? 0 : x - r;
            int y1 = (y - r < 0) ? 0 : y - r;
            int x2 = (x + r >= w) ? w - 1 : x + r;
            int y2 = (y + r >= h) ? h - 1 : y + r;

            int area = (x2 - x1 + 1) * (y2 - y1 + 1);

            uint32_t sumRVal = sumR[y2][x2]
                - (x1 > 0 ? sumR[y2][x1 - 1] : 0)
                - (y1 > 0 ? sumR[y1 - 1][x2] : 0)
                + (x1 > 0 && y1 > 0 ? sumR[y1 - 1][x1 - 1] : 0);

            uint32_t sumGVal = sumG[y2][x2]
                - (x1 > 0 ? sumG[y2][x1 - 1] : 0)
                - (y1 > 0 ? sumG[y1 - 1][x2] : 0)
                + (x1 > 0 && y1 > 0 ? sumG[y1 - 1][x1 - 1] : 0);

            uint32_t sumBVal = sumB[y2][x2]
                - (x1 > 0 ? sumB[y2][x1 - 1] : 0)
                - (y1 > 0 ? sumB[y1 - 1][x2] : 0)
                + (x1 > 0 && y1 > 0 ? sumB[y1 - 1][x1 - 1] : 0);

            outRow[x * 3 + 2] = sumRVal / area;
            outRow[x * 3 + 1] = sumGVal / area;
            outRow[x * 3 + 0] = sumBVal / area;
        }

        for (int p = 0; p < padding; p++) {
            outRow[w * 3 + p] = 0x00;
        }
    }

    fwrite(output, 1, imageSize, fout);
    *escrituras += imageSize;

    fclose(fout);

    free(image);
    free(output);
    for (int i = 0; i < h; i++) {
        free(sumR[i]);
        free(sumG[i]);
        free(sumB[i]);
    }
    free(sumR); free(sumG); free(sumB);
}
