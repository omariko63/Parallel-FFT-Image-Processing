#include <iostream>
#include <fftw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>

#include "stb_image.h"
#include "stb_image_write.h"

int main() {
    auto t_start = std::chrono::high_resolution_clock::now();
    std::cout << "running" << "\n";
    std::cout.flush();
    //Image path
    const char* input_path = "images/input.jpg";
    //load image (request 3 channels)
    int width, height, channels;
    unsigned char* img = stbi_load(input_path, &width, &height, &channels, 3);
    if(!img){
        std::cerr <<"Failed to load image " << input_path << "\n";
        return 1;
    }
    std::cout << "Image loaded: " << width << "x" << height << " channels=" << channels << std::endl;

    const int npix = width * height;

    // Allocate per-channel FFTW input/output arrays
    fftw_complex* inR = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);
    fftw_complex* outR = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);
    fftw_complex* inG = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);
    fftw_complex* outG = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);
    fftw_complex* inB = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);
    fftw_complex* outB = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);

    // Convert interleaved RGB to per-channel complex input
    auto t_RGB_conversion = std::chrono::high_resolution_clock::now();
    #pragma omp parallel for
    for(int i = 0; i < npix; i++){
        inR[i][0] = static_cast<double>(img[3*i + 0]);
        inR[i][1] = 0.0;
        inG[i][0] = static_cast<double>(img[3*i + 1]); 
        inG[i][1] = 0.0;
        inB[i][0] = static_cast<double>(img[3*i + 2]);
         inB[i][1] = 0.0;
    }
    auto t_now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> conversion_time = t_now - t_RGB_conversion;

    stbi_image_free(img);

    // Create forward FFT plans for each channel and execute
    fftw_plan planR_f = fftw_plan_dft_2d(height, width, inR, outR, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan planG_f = fftw_plan_dft_2d(height, width, inG, outG, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan planB_f = fftw_plan_dft_2d(height, width, inB, outB, FFTW_FORWARD, FFTW_ESTIMATE);

    fftw_execute(planR_f);
    fftw_execute(planG_f);
    fftw_execute(planB_f);

    // Highpass filter parameters
    double alpha = 1.0;
    int cx = width / 2;
    int cy = height / 2;
    double max_dist = std::sqrt((double)cx*cx + (double)cy*cy);

    // Apply filter in frequency domain (centered coordinates)
    auto t_filter = std::chrono::high_resolution_clock::now();
    #pragma omp parallel for collapse(2)
    for(int y = 0; y < height; ++y){
        for(int x = 0; x < width; ++x){
            int idx = y * width + x;
            int fx = x; if(fx > cx) fx -= width;
            int fy = y; if(fy > cy) fy -= height;
            double dist = std::sqrt((double)fx*fx + (double)fy*fy);
            double norm = dist / max_dist;
            // Gaussian-style high-pass
            double sigma = 0.25;
            double hp = 1.0 - std::exp(- (norm*norm) / (2.0 * sigma * sigma));
            double factor = 1.0 + alpha * hp;

            outR[idx][0] *= factor; outR[idx][1] *= factor;
            outG[idx][0] *= factor; outG[idx][1] *= factor;
            outB[idx][0] *= factor; outB[idx][1] *= factor;
        }
    }
    t_now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> filter_time = t_now - t_filter;

    // Prepare reverse buffers and backward plans
    fftw_complex* revR = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);
    fftw_complex* revG = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);
    fftw_complex* revB = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * npix);

    fftw_plan planR_b = fftw_plan_dft_2d(height, width, outR, revR, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_plan planG_b = fftw_plan_dft_2d(height, width, outG, revG, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_plan planB_b = fftw_plan_dft_2d(height, width, outB, revB, FFTW_BACKWARD, FFTW_ESTIMATE);

    fftw_execute(planR_b);
    fftw_execute(planG_b);
    fftw_execute(planB_b);

    // Normalize inverse FFT output
    auto t_normalization = std::chrono::high_resolution_clock::now();
    #pragma omp parallel for collapse(2)
    for(int y = 0; y < height; ++y){
        for(int x = 0; x < width; ++x){
            int idx = y * width + x;
            revR[idx][0] /= (npix);
            revG[idx][0] /= (npix);
            revB[idx][0] /= (npix);
        }
    }
    t_now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> normalization_time = t_now - t_normalization;

    // Convert back to interleaved 8-bit RGB
    unsigned char* output_image = new unsigned char[npix * 3];
    auto t_reverse_conversion = std::chrono::high_resolution_clock::now();
    #pragma omp parallel for
    for(int i = 0; i < npix; ++i){
        double r = std::round(revR[i][0]);
        double g = std::round(revG[i][0]);
        double b = std::round(revB[i][0]);
        r = std::max(0.0, std::min(r, 255.0));
        g = std::max(0.0, std::min(g, 255.0));
        b = std::max(0.0, std::min(b, 255.0));
        output_image[3*i + 0] = static_cast<unsigned char>(r);
        output_image[3*i + 1] = static_cast<unsigned char>(g);
        output_image[3*i + 2] = static_cast<unsigned char>(b);
    }
    t_now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> reverse_time = t_now - t_reverse_conversion;

    stbi_write_png("images/output.png", width, height, 3, output_image, width * 3);
    auto t_end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = t_end - t_start;
    std::cout << "Total execution time: "
          << elapsed.count() << " seconds\n";
    std::cout << "RGB conversion: " << conversion_time.count() << " s\n";
    std::cout << "Frequency filter: " << filter_time.count() << " s\n";
    std::cout << "Normalization: " << normalization_time.count() << " s\n";
    std::cout << "Reverse conversion: " << reverse_time.count() << " s\n";

    // Cleanup
    delete[] output_image;
    fftw_destroy_plan(planR_b); fftw_destroy_plan(planG_b); fftw_destroy_plan(planB_b);
    fftw_destroy_plan(planR_f); fftw_destroy_plan(planG_f); fftw_destroy_plan(planB_f);
    fftw_free(revR); fftw_free(revG); fftw_free(revB);
    fftw_free(inR); fftw_free(outR); fftw_free(inG); fftw_free(outG); fftw_free(inB); fftw_free(outB);

    return 0;
}