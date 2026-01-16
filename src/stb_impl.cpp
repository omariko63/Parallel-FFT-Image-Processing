#include <fft_processor.h>
#include <iostream>
#include <iomanip>


#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

// memory management
unsigned char* load_image(const char* filename, int& width, int& height, int& channels) {
    std::cout << "Reading from: " << filename << "\n";
    
    unsigned char* img = stbi_load(filename, &width, &height, &channels, 3);
    
    if (!img) {
        std::cerr << "Error loading image: " << stbi_failure_reason() << "\n";
        return nullptr;
    }
    
    std::cout << "Image loaded: " << width << "x" << height << " pixels\n";
    std::cout << "Total pixels: " << (width * height) << "\n";
    
    return img;
}

void free_image(unsigned char* img) {
    if (img) {
        stbi_image_free(img);
    }
}

// fttw3 memory management
fftw_complex* allocate_fftw_array(int size) {
    // FFTW needs special memory alignment for SIMD for performance reasons 
    fftw_complex* arr = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size);
    
    if (!arr) {
        std::cerr << "Error: Failed to allocate FFTW array of size " << size << "\n";
        return nullptr;
    }
    
    // initialize to zero
    #pragma omp parallel for schedule(static, 256)
    for(int i = 0; i < size; i++) {
        arr[i][0] = 0.0;
        arr[i][1] = 0.0;
    }
    
    return arr;
}

void free_fftw_array(fftw_complex* arr) {
    if (arr) {
        fftw_free(arr);
    }
}

// output writing
void write_output_image(const char* filename, unsigned char* img, int width, int height) {
    std::cout << "Writing output image...\n";
    std::cout << "  File: " << filename << "\n";
    
    int success = stbi_write_png(filename, width, height, 3, img, width * 3);
    
    if (success) {
        std::cout << "Image written successfully\n";
    } else {
        std::cerr << "Error writing image\n";
    }
}