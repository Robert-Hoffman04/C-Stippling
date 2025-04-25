#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


typedef struct {
    int width;
    int height;
    float* data;
} FloatImage;

FloatImage loadImage(const char* filename) {
    FloatImage img = {0, 0, NULL};

    int channels;
    unsigned char *data = stbi_load(filename, &img.width, &img.height, &channels, 1);
    if (!data)
    {
        fprintf(stderr, "Failed to load image: %s\n", filename);
        return img; // img.data is NULL, indicating failure
    }

    int size = img.width * img.height;
    img.data = (float*) malloc(size * sizeof(float));
    if (!img.data)
    {
        fprintf(stderr, "Memory allocation failed\n");
        stbi_image_free(data);
        return img;
    }

    for (int i = 0; i < size; i++) {
        img.data[i] = 1 - (data[i] / 255.0f);
    }

    stbi_image_free(data);
    return img;
}

