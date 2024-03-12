//
// Created by Alex Flores Escarcega on 3/10/24.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

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

int encode_splats_play_canvas_format(Vertex* vertices, int num_vertices, int coalesced_num_vertices, FILE* file) {
    char header[1024];
    sprintf(header, PLAY_CANVAS_PLY_HEADER, coalesced_num_vertices);
    fwrite(header, 1, strlen(header), file);

    for (int i = 0; i < num_vertices; i++) {
        if (vertices[i].opacity != 0.0f) {
            fwrite(&vertices[i], sizeof(Vertex), 1, file);
        }
    }

    return ftell(file);
}

void print_help() {
    printf("Usage: gaussian_splats [options] <image_path> [depth_map_path]\n");
    printf("Options:\n");
    printf("  -h, --help       Show this help message and exit\n");
    printf("  -o, --output     Specify the output file path\n");
}

int main(int argc, char* argv[]) {
    char output_path[256];
    sprintf(output_path, "%s%s", OUTPUT_DIR, OUTPUT_PLY_NAME);

    int opt;
    static struct option long_options[] = {
            {"help", no_argument, 0, 'h'},
            {"output", required_argument, 0, 'o'},
            {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "ho:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_help();
                return 0;
            case 'o':
                strcpy(output_path, optarg);
                break;
            default:
                print_help();
                return 1;
        }
    }

    if (optind >= argc || optind + 2 < argc) {
        print_help();
        return 1;
    }

    const char* image_path = argv[optind];
    const char* depth_map_path = (optind + 1 < argc) ? argv[optind + 1] : NULL;

    clock_t start_time = clock();

    int width, height, channels;
    unsigned char* image_data = stbi_load(image_path, &width, &height, &channels, 3);
    if (!image_data) {
        printf("Failed to load image.\n");
        return 1;
    }

    int depth_width = 0, depth_height = 0, depth_channels = 0;
    unsigned char* depth_data = NULL;
    if (depth_map_path != NULL) {
        depth_data = stbi_load(depth_map_path, &depth_width, &depth_height, &depth_channels, 1);
        if (!depth_data || depth_width != width || depth_height != height) {
            printf("Failed to load depth map or dimensions mismatch.\n");
            stbi_image_free(image_data);
            return 1;
        }
        printf("Using depth information.\n");
    }

    int num_vertices = width * height;
    Vertex* vertices = (Vertex*)malloc(num_vertices * sizeof(Vertex));

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            vertices[index].packed_position[0] = (float)x;
            vertices[index].packed_position[1] = (float)y;

            if (depth_data) {
                float depth = depth_data[index];
                vertices[index].packed_position[2] = depth;
            } else {
                vertices[index].packed_position[2] = FLAT ? 0.0f : 0.0f;
            }

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

    int coalesced_num_vertices = num_vertices;

    if (!depth_data) {
        // Coalesce adjacent vertices of the same color only if there's no depth map
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int index = y * width + x;
                Vertex* current = &vertices[index];

                if (current->opacity == 0.0f) {
                    continue; // Skip already coalesced vertices
                }

                // Check the right neighbor
                if (x < width - 1) {
                    Vertex* right = &vertices[index + 1];
                    if (memcmp(current->packed_color, right->packed_color, sizeof(float) * 3) == 0) {
                        // Colors match, coalesce the vertices
                        current->packed_position[0] = (current->packed_position[0] + right->packed_position[0]) / 2.0f;
                        current->packed_scale[0] *= 2.0f;
                        right->opacity = 0.0f; // Mark the right vertex as coalesced
                    }
                }

                // Check the bottom neighbor
                if (y < height - 1) {
                    Vertex* bottom = &vertices[index + width];
                    if (memcmp(current->packed_color, bottom->packed_color, sizeof(float) * 3) == 0) {
                        // Colors match, coalesce the vertices
                        current->packed_position[1] = (current->packed_position[1] + bottom->packed_position[1]) / 2.0f;
                        current->packed_scale[1] *= 2.0f;
                        bottom->opacity = 0.0f; // Mark the bottom vertex as coalesced
                    }
                }
            }
        }

        // Count the number of remaining vertices after coalescing
        coalesced_num_vertices = 0;
        for (int i = 0; i < num_vertices; i++) {
            if (vertices[i].opacity != 0.0f) {
                coalesced_num_vertices++;
            }
        }
    }

    FILE* file = fopen(output_path, "wb");
    if (!file) {
        printf("Failed to open output file.\n");
        free(vertices);
        stbi_image_free(image_data);
        if (depth_data) {
            stbi_image_free(depth_data);
        }
        return 1;
    }

    int bytes_written = encode_splats_play_canvas_format(vertices, num_vertices, coalesced_num_vertices, file);
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
    if (depth_data) {
        stbi_image_free(depth_data);
    }

    return 0;
}