// image_processing.h
#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t importantColors;
} DIBHeader;
#pragma pack(pop)

void invertirHorizontalGrises(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras);
void invertirHorizontalColor(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras);
void aplicarDesenfoqueIntegral(const char *entrada, const char *salida, int kernelSize, FILE *log, unsigned long *lecturas, unsigned long *escrituras);
void invertirVerticalGrises(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras);
void invertirVerticalColor(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras);
void convertirAGrises(const char *entrada, const char *salida, FILE *log, unsigned long *lecturas, unsigned long *escrituras);
#endif // IMAGE_PROCESSING_H