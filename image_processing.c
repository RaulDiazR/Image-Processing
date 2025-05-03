// image_processing.c
#include "image_processing.h"

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
    uint8_t *row = malloc(dib.width * 3);
    if (!row) {
        fclose(fin); fclose(fout);
        logError(log, "Memoria insuficiente.");
        return;
    }

    for (int y = 0; y < dib.height; y++) {
        fread(row, 3, dib.width, fin);
        *lecturas += dib.width * 3;
        fseek(fin, padding, SEEK_CUR);
        *lecturas += padding;

        for (int x = dib.width - 1; x >= 0; x--) {
            uint8_t r = row[x*3+2];
            uint8_t g = row[x*3+1];
            uint8_t b = row[x*3+0];
            uint8_t gray = (uint8_t)(0.21*r + 0.72*g + 0.07*b);

            fputc(gray, fout);
            fputc(gray, fout);
            fputc(gray, fout);
            *escrituras += 3;
        }

        for (int p = 0; p < padding; p++) {
            fputc(0x00, fout);
            *escrituras += 1;
        }
    }

    free(row);
    fclose(fin);
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
    uint8_t *row = malloc(dib.width * 3);
    if (!row) {
        fclose(fin);
        fclose(fout);
        logError(log, "Memoria insuficiente.");
        return;
    }

    for (int y = 0; y < dib.height; y++) {
        fread(row, 3, dib.width, fin);
        *lecturas += dib.width * 3;
        fseek(fin, padding, SEEK_CUR);
        *lecturas += padding;

        for (int x = dib.width - 1; x >= 0; x--) {
            fputc(row[x*3+0], fout);
            fputc(row[x*3+1], fout);
            fputc(row[x*3+2], fout);
            *escrituras += 3;
        }

        for (int p = 0; p < padding; p++) {
            fputc(0x00, fout);
            *escrituras += 1;
        }
    }

    free(row);
    fclose(fin);
    fclose(fout);
}
