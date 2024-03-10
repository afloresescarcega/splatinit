//
// Created by Alex Flores Escarcega on 3/10/24.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define FLAT 0
#define GLOBAL_SCALE 1
#define OUTPUT_DIR "/tmp/splatting/"
#define OUTPUT_PLY_NAME "scene_play_canvas_format.ply"

const char* PLAY_CANVAS_PLY_HEADER = "ply\n"
                                     "format binary_little_endian 1.0\n"
                                     "element vertex %d\n"
                                     "property float x\n"
                                     "property float y\n"
                                     "property float z\n"
                                     "property float f_dc_0\n"
                                     "property float f_dc_1\n"
                                     "property float f_dc_2\n"
                                     "property float opacity\n"
                                     "property float rot_0\n"
                                     "property float rot_1\n"
                                     "property float rot_2\n"
                                     "property float rot_3\n"
                                     "property float scale_0\n"
                                     "property float scale_1\n"
                                     "property float scale_2\n"
                                     "end_header\n";

const float C0 = 0.28209479177387814f;

typedef struct {
    float packed_position[3];
    float packed_color[3];
    float opacity;
    float packed_rotation[4];
    float packed_scale[3];
} Vertex;

void rgb2_sh(float rgb[3], float sh[3]) {
    for (int i = 0; i < 3; i++) {
        sh[i] = (rgb[i] - 0.5f) / C0;
    }
}

int encode_splats_play_canvas_format(Vertex* vertices, int num_vertices, FILE* file) {
    char header[1024];
    sprintf(header, PLAY_CANVAS_PLY_HEADER, num_vertices);
    fwrite(header, 1, strlen(header), file);

    for (int i = 0; i < num_vertices; i++) {
        fwrite(&vertices[i], sizeof(Vertex), 1, file);
    }

    return ftell(file);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <image_path>\n", argv[0]);
        return 1;
    }

    clock_t start_time = clock();

    int width, height, channels;
    unsigned char* image_data = stbi_load(argv[1], &width, &height, &channels, 3);
    if (!image_data) {
        printf("Failed to load image.\n");
        return 1;
    }

    int num_vertices = width * height;
    Vertex* vertices = (Vertex*)malloc(num_vertices * sizeof(Vertex));

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            vertices[index].packed_position[0] = (float)x;
            vertices[index].packed_position[1] = (float)y;
            vertices[index].packed_position[2] = FLAT ? 0.0f : 0.0f; // TODO: Generate depth map

            float rgb[3];
            for (int c = 0; c < 3; c++) {
                rgb[c] = image_data[(y * width + x) * 3 + c] / 255.0f;
            }
            rgb2_sh(rgb, vertices[index].packed_color);

            vertices[index].opacity = 1.0f;
            vertices[index].packed_rotation[0] = 1.0f;
            vertices[index].packed_rotation[1] = 1.0f;
            vertices[index].packed_rotation[2] = 0.0f;
            vertices[index].packed_rotation[3] = 0.0f;
            vertices[index].packed_scale[0] = 0.1f;
            vertices[index].packed_scale[1] = 0.1f;
            vertices[index].packed_scale[2] = 0.1f;
        }
    }

    char output_path[256];
    sprintf(output_path, "%s%s", OUTPUT_DIR, OUTPUT_PLY_NAME);
    FILE* file = fopen(output_path, "wb");
    if (!file) {
        printf("Failed to open output file.\n");
        free(vertices);
        stbi_image_free(image_data);
        return 1;
    }

    int bytes_written = encode_splats_play_canvas_format(vertices, num_vertices, file);
    fclose(file);

    printf("Output file: %s\n", output_path);
    printf("Bytes written: %d\n", bytes_written);
    printf("Original image size: %d bytes\n", width * height * channels);
    printf("Bytes per original byte: %.2f\n", (float)bytes_written / (width * height * channels));

    clock_t end_time = clock();
    double execution_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Execution time: %.2f seconds\n", execution_time);
    printf("Execution time over 1hz: %.2f times\n", execution_time / 0.01667);

    free(vertices);
    stbi_image_free(image_data);

    return 0;
}