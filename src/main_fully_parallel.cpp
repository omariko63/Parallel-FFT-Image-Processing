#include "fft_processor.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <omp.h>

const char* OUTPUT_PATH = "../images/output_fully_parallel.png";

// parallel execution of forward FFT
// R, G, B FFTs are executed in parallel on separate threads/cores -> better utilization of multi-core CPUs (eliminates bottleneck in sequential version)
void execute_forward_fft_parallel(fftw_plan planR_f, fftw_plan planG_f, fftw_plan planB_f) {
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            fftw_execute(planR_f);  // Core 0
        }
        #pragma omp section
        {
            fftw_execute(planG_f);  // Core 1
        }
        #pragma omp section
        {
            fftw_execute(planB_f);  // Core 2
        }
    }
}

// parallel execution of inverse FFT
void execute_inverse_fft_parallel(fftw_plan planR_b, fftw_plan planG_b, fftw_plan planB_b) {
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            fftw_execute(planR_b);  // Core 0
        }
        #pragma omp section
        {
            fftw_execute(planG_b);  // Core 1
        }
        #pragma omp section
        {
            fftw_execute(planB_b);  // Core 2
        }
    }
}

// parallel filtering in the frequency domain
// loop parallelization with collapse (2) for 2D loops to distribute work across multiple threads/cores
void apply_frequency_filter_parallel(fftw_complex* outR, fftw_complex* outG, fftw_complex* outB, int width, int height, double alpha, double sigma) {
    const double sigma_sq = sigma * sigma;
    const int npix = width * height;

    const int cx = width / 2;
    const int cy = height / 2;
    const double max_dist = std::sqrt((double)cx*cx + (double)cy*cy);
    
    #pragma omp parallel for collapse(2) schedule(static, 64)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int idx = y * width + x; 

            // distance from center
            int fx = x; if(fx > cx) fx -= width;
            int fy = y; if(fy > cy) fy -= height;

            // Normalize distance to [0, 1]
            double dist = std::sqrt((double)fx*fx + (double)fy*fy);
            double norm = dist / max_dist;

            // gaussian high-pass filter
            double hp = 1.0 - std::exp(-(norm*norm) / (2.0 * sigma * sigma));
            double factor = 1.0 + alpha * hp;
            

            // apply filter to all color channels
            outR[idx][0] *= factor;
            outR[idx][1] *= factor;
            outG[idx][0] *= factor;
            outG[idx][1] *= factor;
            outB[idx][0] *= factor;
            outB[idx][1] *= factor;
        }
    }
}

// normalize the inverse FFT output in parallel with SIMD-friendly loop parallelization
void normalize_output_parallel(fftw_complex* revR, fftw_complex* revG, fftw_complex* revB, int npix) {
    #pragma omp parallel for schedule(static, 256)
    for(int i = 0; i < npix; i++) {
        const double inv_npix = 1.0 / npix;
        revR[i][0] *= inv_npix;
        revR[i][1] *= inv_npix;
        revG[i][0] *= inv_npix;
        revG[i][1] *= inv_npix;
        revB[i][0] *= inv_npix;
        revB[i][1] *= inv_npix;
    }
}

// convert from uint8 RGB format to FFTW complex format in parallel
void convert_to_fftw_complex_parallel(unsigned char* img, int npix, fftw_complex* inR, fftw_complex* inG, fftw_complex* inB) {
    #pragma omp parallel for schedule(static, 256)
    for(int i = 0; i < npix; i++) {
        inR[i][0] = img[i * 3 + 0];
        inR[i][1] = 0.0;
        inG[i][0] = img[i * 3 + 1];
        inG[i][1] = 0.0;
        inB[i][0] = img[i * 3 + 2];
        inB[i][1] = 0.0;
    }
}

// convert FFTW complex output to uint8 RGB format in parallel
unsigned char* convert_to_uint8_parallel(fftw_complex* revR, fftw_complex* revG, fftw_complex* revB, int npix) {
    unsigned char* output = new unsigned char[npix * 3];
    
    #pragma omp parallel for schedule(static, 256)
    for(int i = 0; i < npix; i++) {
        double r = std::round(revR[i][0]);
        double g = std::round(revG[i][0]);
        double b = std::round(revB[i][0]);
        
        r = std::max(0.0, std::min(r, 255.0));
        g = std::max(0.0, std::min(g, 255.0));
        b = std::max(0.0, std::min(b, 255.0));
        
        output[i * 3 + 0] = static_cast<unsigned char>(r);
        output[i * 3 + 1] = static_cast<unsigned char>(g);
        output[i * 3 + 2] = static_cast<unsigned char>(b);
    }
    
    return output;
}

int main() {
    auto t_start = std::chrono::high_resolution_clock::now();
    TimingInfo timing;

    std::cout << "Starting Fully Parallel FFT Image Processing...\n";
    std::cout << "OpenMP Threads Available:" << omp_get_max_threads() << "\n";

    // load image from file
     int width, height, channels;
    unsigned char* img = load_image(INPUT_PATH, width, height, channels);
    if (!img) return 1;
    
    const int npix = width * height;
    
    // memory allocation for FFTW input and output arrays for R, G, B channels
    fftw_complex* inR = allocate_fftw_array(npix);
    fftw_complex* outR = allocate_fftw_array(npix);
    fftw_complex* inG = allocate_fftw_array(npix);
    fftw_complex* outG = allocate_fftw_array(npix);
    fftw_complex* inB = allocate_fftw_array(npix);
    fftw_complex* outB = allocate_fftw_array(npix);
    fftw_complex* revR = allocate_fftw_array(npix);
    fftw_complex* revG = allocate_fftw_array(npix);
    fftw_complex* revB = allocate_fftw_array(npix);

    // convert RGB image to FFTW complex format in parallel
    std::cout << "Converting RGB to FFTW complex format in parallel...\n";
    auto t_rgb = std::chrono::high_resolution_clock::now();
    convert_to_fftw_complex_parallel(img, npix, inR, inG, inB);
    auto t_rgb_end = std::chrono::high_resolution_clock::now();
    timing.rgb_time = (t_rgb_end - t_rgb).count();
    
    free_image(img);

    // create forward FFT plans and execute forward FFTs in parallel
    std::cout << "Initializing FFTW threading...\n";
    fftw_init_threads();
    fftw_plan_with_nthreads(omp_get_max_threads());

    std::cout << "Creating FFT plans (Parallel + Measure Planning)...\n";
    // use FFTW_MEASURE for better performance with threading (more optimization but longer planning time)
    fftw_plan planR_f = fftw_plan_dft_2d(height, width, inR, outR, FFTW_FORWARD, FFTW_MEASURE);
    fftw_plan planG_f = fftw_plan_dft_2d(height, width, inG, outG, FFTW_FORWARD, FFTW_MEASURE);
    fftw_plan planB_f = fftw_plan_dft_2d(height, width, inB, outB, FFTW_FORWARD, FFTW_MEASURE);
    
    std::cout << "Executing Forward FFT (PARALLEL sections - R ∥ G ∥ B)...\n";
    auto t_fft = std::chrono::high_resolution_clock::now();
    execute_forward_fft_parallel(planR_f, planG_f, planB_f);
    auto t_fft_end = std::chrono::high_resolution_clock::now();
    timing.fft_time = (t_fft_end - t_fft).count();

    // apply frequency domain filter in parallel
    std::cout << "Applying frequency filter in parallel...\n";
    auto t_filter = std::chrono::high_resolution_clock::now();
    apply_frequency_filter_parallel(outR, outG, outB, width, height, FILTER_ALPHA, FILTER_SIGMA);
    auto t_filter_end = std::chrono::high_resolution_clock::now();
    timing.filter_time = (t_filter_end - t_filter).count();
    
    // create inverse FFT plans and execute inverse FFTs in parallel
    std::cout << "Creating inverse FFT plans (Parallel + Measure Planning)...\n";
    fftw_plan planR_b = fftw_plan_dft_2d(height, width, outR, revR, FFTW_BACKWARD, FFTW_MEASURE);
    fftw_plan planG_b = fftw_plan_dft_2d(height, width, outG, revG, FFTW_BACKWARD, FFTW_MEASURE);
    fftw_plan planB_b = fftw_plan_dft_2d(height, width, outB, revB, FFTW_BACKWARD, FFTW_MEASURE);
    
    std::cout << "Executing Inverse FFT (PARALLEL sections - R ∥ G ∥ B)...\n";
    auto t_ifft = std::chrono::high_resolution_clock::now();
    execute_inverse_fft_parallel(planR_b, planG_b, planB_b);
    auto t_ifft_end = std::chrono::high_resolution_clock::now();
    timing.ifft_time = (t_ifft_end - t_ifft).count();

    // normalize inverse FFT output in parallel
    std::cout << "Normalizing output in parallel...\n";
    auto t_norm = std::chrono::high_resolution_clock::now();
    normalize_output_parallel(revR, revG, revB, npix);
    auto t_norm_end = std::chrono::high_resolution_clock::now();
    timing.norm_time = (t_norm_end - t_norm).count();

    // convert back to uint8 RGB format and write output image in parallel 
    std::cout << "Converting FFTW complex output to uint8 RGB format in parallel...\n";
    auto t_conv = std::chrono::high_resolution_clock::now();
    unsigned char* output_image = convert_to_uint8_parallel(revR, revG, revB, npix);
    auto t_conv_end = std::chrono::high_resolution_clock::now();
    timing.conv_time = (t_conv_end - t_conv).count();
    
    write_output_image(OUTPUT_PATH, output_image, width, height);
    
    // cleanup
    delete[] output_image;
    
    fftw_destroy_plan(planR_b);
    fftw_destroy_plan(planG_b);
    fftw_destroy_plan(planB_b);
    fftw_destroy_plan(planR_f);
    fftw_destroy_plan(planG_f);
    fftw_destroy_plan(planB_f);
    
    free_fftw_array(revR);
    free_fftw_array(revG);
    free_fftw_array(revB);
    free_fftw_array(inR);
    free_fftw_array(outR);
    free_fftw_array(inG);
    free_fftw_array(outG);
    free_fftw_array(inB);
    free_fftw_array(outB);
    
    fftw_cleanup_threads();
    
    // results
    auto t_end = std::chrono::high_resolution_clock::now();
    timing.total_time = (t_end - t_start).count();
    
    timing.print_summary("FULLY PARALLEL");
    
    return 0;
}