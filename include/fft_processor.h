// declarations for all three versions and shared utilities

#ifndef FFT_PROCESSOR_H
#define FFT_PROCESSOR_H

#include <fftw3.h>
#include <cmath>
#include <iostream>
#include <iomanip>

// config constants
#define INPUT_PATH "../images/input.jpg"
#define FILTER_ALPHA 1.0
#define FILTER_SIGMA 0.25

// timing information struct
struct TimingInfo {
    long long rgb_time = 0;      // RGB conversion time (nanoseconds)
    long long fft_time = 0;      // forward FFT time
    long long filter_time = 0;   // frequency domain filtering time
    long long ifft_time = 0;     // inverse FFT time
    long long norm_time = 0;     // normalization time
    long long conv_time = 0;     // output conversion time
    long long total_time = 0;    // total time
    
    void print_summary(const char* version_name) const {
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  " << version_name << " - Performance Summary                    ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
        
        // Convert nanoseconds to seconds
        const double rgb_s = rgb_time / 1000000000.0;
        const double fft_s = fft_time / 1000000000.0;
        const double filter_s = filter_time / 1000000000.0;
        const double ifft_s = ifft_time / 1000000000.0;
        const double norm_s = norm_time / 1000000000.0;
        const double conv_s = conv_time / 1000000000.0;
        const double total_s = total_time / 1000000000.0;

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "RGB Conversion:    " << std::setw(8) << rgb_s << " s\n";
        std::cout << "FFT Forward:       " << std::setw(8) << fft_s << " s\n";
        std::cout << "Frequency Filter:  " << std::setw(8) << filter_s << " s\n";
        std::cout << "FFT Inverse:       " << std::setw(8) << ifft_s << " s\n";
        std::cout << "Normalization:     " << std::setw(8) << norm_s << " s\n";
        std::cout << "Output Conversion: " << std::setw(8) << conv_s << " s\n";
        std::cout << "─────────────────────────────\n";
        std::cout << "Total Time:        " << std::setw(8) << total_s << " s\n\n";

    }
};

// shared utility function declarations
// image loading
unsigned char* load_image(const char* filename, int& width, int& height, int& channels);
void free_image(unsigned char* img);

// memory allocation/deallocation
fftw_complex* allocate_fftw_array(int size);
void free_fftw_array(fftw_complex* arr);

// output writing
void write_output_image(const char* filename, unsigned char* img, int width, int height);

// fully sequential version - function declarations
void execute_forward_fft_sequential(fftw_plan planR_f, fftw_plan planG_f, fftw_plan planB_f);
void execute_inverse_fft_sequential(fftw_plan planR_b, fftw_plan planG_b, fftw_plan planB_b);
void apply_frequency_filter_sequential(fftw_complex* outR, fftw_complex* outG, fftw_complex* outB,
                                      int width, int height, double alpha, double sigma);
void normalize_output_sequential(fftw_complex* revR, fftw_complex* revG, fftw_complex* revB, int npix);

// fully parallel version - function declarations
void execute_forward_fft_parallel(fftw_plan planR_f, fftw_plan planG_f, fftw_plan planB_f);
void execute_inverse_fft_parallel(fftw_plan planR_b, fftw_plan planG_b, fftw_plan planB_b);
void apply_frequency_filter_parallel(fftw_complex* outR, fftw_complex* outG, fftw_complex* outB,
                                    int width, int height, double alpha, double sigma);
void normalize_output_parallel(fftw_complex* revR, fftw_complex* revG, fftw_complex* revB, int npix);
void convert_to_fftw_complex_parallel(unsigned char* img, int npix,
                                    fftw_complex* inR, fftw_complex* inG, fftw_complex* inB);
unsigned char* convert_to_uint8_parallel(fftw_complex* revR, fftw_complex* revG, 
                                        fftw_complex* revB, int npix);

#endif