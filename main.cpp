#include <iostream>
#include <fftw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "stb_image.h"
#include "stb_image_write.h"


//SINGLE CHANNEL ONLY -- Used in ensuring the pipeline works correctly

int main() {
    std::cout << "running" << "\n";
    std::cout.flush();
    //Image path
    const char* input_path = "images/input.jpg";
    //load image
    int width, height, channels;
    unsigned char* img = stbi_load(input_path, &width, &height, &channels, 1);
    if(!img){
        std::cerr <<"Failed to load image" << input_path << "\n";
        return 1;
    }
    std::cout << "Image loaded: " << width << "x" << height << std::endl;

    //Allocate FFTW Input array
    fftw_complex* in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * width * height);
    for(int i = 0; i < width * height; i++ ){
        in[i][0] = static_cast<double>(img[i]);
        in[i][1] = 0.0;
    }
    stbi_image_free(img);

    //Allocate output array
    fftw_complex* out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * width * height);

    //Create 2D FFT Plan
    fftw_plan plan_forward = fftw_plan_dft_2d(
        height, width,
        in,
        out,
        FFTW_FORWARD,
        FFTW_ESTIMATE
    );

    fftw_execute(plan_forward);


    //Highpass filter
    double alpha = 1.0;
    int cx = width / 2;
    int cy = height / 2;


    #pragma omp parallel for collapse(2)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            
            int dx = x - cx;
            int dy = y - cy;
            double dist = std::sqrt(dx*dx + dy*dy);

            double max_dist = std::sqrt(cx*cx + cy*cy);
            double norm_dist = dist / max_dist;

            double factor = 1.0 + alpha * norm_dist;
            out[idx][0] *= factor;
            out[idx][1] *= factor;

        }
    }

    fftw_complex* reverse = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * width * height);
    fftw_plan plan_backwards = fftw_plan_dft_2d(
        height,width,
        out,
        reverse,
        FFTW_BACKWARD,
        FFTW_ESTIMATE
    );

    fftw_execute(plan_backwards);

    //Normalizing output
    #pragma omp parallel for collapse(2)
    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            int idx = y * width + x;
            reverse[idx][0] /= (width * height);
            reverse[idx][1] /= (width * height);
        }
    }

    //conversion to 8-bit greyscale
    unsigned char* output_image = new unsigned char[width * height];
    #pragma omp parallel for
    for(int i = 0; i < width * height; i++){
        double val = reverse[i][0];
        val = std::max(0.0, std::min(val,255.0));
        output_image[i] = static_cast<unsigned char>(val);
    }

    stbi_write_png("images/output.png", width, height, 1, output_image, width);

    //cleanup
    delete[] output_image;
    fftw_destroy_plan(plan_backwards);
    fftw_destroy_plan(plan_forward);
    fftw_free(reverse);
    fftw_free(in);
    fftw_free(out);



    return 0;
}