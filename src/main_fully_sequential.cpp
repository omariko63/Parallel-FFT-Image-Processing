#include "fft_processor.h"
#include <iostream>
#include <chrono>
#include <iomanip>

const char* OUTPUT_PATH = "../images/output_fully_sequential.png";

// sequential execution of forward FFT
// R, G, B FFTs are executed one after another on the same core -> bottleneck in sequential version
void execute_forward_fft_sequential(fftw_plan planR_f, fftw_plan planG_f, fftw_plan planB_f) {
    fftw_execute(planR_f);
    fftw_execute(planG_f);
    fftw_execute(planB_f);
}

// sequential execution of inverse FFT
void execute_inverse_fft_sequential(fftw_plan planR_b, fftw_plan planG_b, fftw_plan planB_b) {
    fftw_execute(planR_b);
    fftw_execute(planG_b);
    fftw_execute(planB_b);
}

// sequential filtering in the frequency domain
void apply_frequency_filter_sequential(fftw_complex* outR, fftw_complex* outG, fftw_complex* outB, int width, int height, double alpha, double sigma) {
    const double sigma_sq = sigma * sigma;
    const int npix = width * height;

    const int cx = width / 2;
    const int cy = height / 2;
    const double max_dist = std::sqrt((double)cx*cx + (double)cy*cy);

    // sequential loop over all pixels
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

// sequential normalization of the inverse FFT output
void normalize_output_sequential(fftw_complex* revR, fftw_complex* revG, fftw_complex* revB, int npix) {
    for (int i = 0; i < npix; i++) {
        revR[i][0] /= npix;
        revR[i][1] /= npix;
        revG[i][0] /= npix;
        revG[i][1] /= npix;
        revB[i][0] /= npix;
        revB[i][1] /= npix;
    }
}

int main() {
    auto t_start = std::chrono::high_resolution_clock::now();
    TimingInfo timing;

    std::cout << "Starting Fully Sequential FFT Image Processing...\n";

    // load image from file
    int width, height, channels;
    unsigned char* img = load_image(INPUT_PATH, width, height, channels);
    if (!img) {
        return 1;
    }

    const int npix = width * height;

    // allocate FFTW input and output arrays for R, G, B channels
    fftw_complex* inR = allocate_fftw_array(npix);
    fftw_complex* outR = allocate_fftw_array(npix);
    fftw_complex* inG = allocate_fftw_array(npix);
    fftw_complex* outG = allocate_fftw_array(npix);
    fftw_complex* inB = allocate_fftw_array(npix);
    fftw_complex* outB = allocate_fftw_array(npix);
    fftw_complex* revR = allocate_fftw_array(npix);
    fftw_complex* revG = allocate_fftw_array(npix);
    fftw_complex* revB = allocate_fftw_array(npix);

    // RGB conversion to FFTW complex format
    std::cout << "Converting RGB to FFTW complex format...\n";
    auto t_rgb = std::chrono::high_resolution_clock::now();

    // manual conversion loop
    for (int i = 0; i < npix; i++) {
        inR[i][0] = static_cast<double>(img[3 * i + 0]);
        inR[i][1] = 0.0;
        inG[i][0] = static_cast<double>(img[3 * i + 1]);
        inG[i][1] = 0.0;
        inB[i][0] = static_cast<double>(img[3 * i + 2]);
        inB[i][1] = 0.0;
    }

    auto t_rgb_end = std::chrono::high_resolution_clock::now();
    timing.rgb_time = (t_rgb_end - t_rgb).count();

    free_image(img);

    // create forward FFTW plans and execute forward FFTs
    std::cout << "Creating FFT plans (FFTW_ESTIMATE - quick planning)...\n";

    // use FFTW_ESTIMATE for quick planning in sequential version; not optimal but faster planning time
    fftw_plan planR_f = fftw_plan_dft_2d(height, width, inR, outR, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan planG_f = fftw_plan_dft_2d(height, width, inG, outG, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan planB_f = fftw_plan_dft_2d(height, width, inB, outB, FFTW_FORWARD, FFTW_ESTIMATE);

    std::cout << "Executing forward FFTs sequentially...\n";
     auto t_fft = std::chrono::high_resolution_clock::now();
    execute_forward_fft_sequential(planR_f, planG_f, planB_f);
    auto t_fft_end = std::chrono::high_resolution_clock::now();
    timing.fft_time = (t_fft_end - t_fft).count();

    // applying frequency filter
    std::cout << "Applying frequency filter sequentially...\n";
    auto t_filter = std::chrono::high_resolution_clock::now();
    apply_frequency_filter_sequential(outR, outG, outB, width, height, FILTER_ALPHA, FILTER_SIGMA);
    auto t_filter_end = std::chrono::high_resolution_clock::now();
    timing.filter_time = (t_filter_end - t_filter).count();

    // inverse FFT plans and execution
    std::cout << "Creating inverse FFT plans (FFTW_ESTIMATE - quick planning)...\n";
    fftw_plan planR_b = fftw_plan_dft_2d(height, width, outR, revR, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_plan planG_b = fftw_plan_dft_2d(height, width, outG, revG, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_plan planB_b = fftw_plan_dft_2d(height, width, outB, revB, FFTW_BACKWARD, FFTW_ESTIMATE);
    
    std::cout << "Executing inverse FFTs sequentially...\n";
    auto t_ifft = std::chrono::high_resolution_clock::now();
    execute_inverse_fft_sequential(planR_b, planG_b, planB_b);
    auto t_ifft_end = std::chrono::high_resolution_clock::now();
    timing.ifft_time = (t_ifft_end - t_ifft).count();

    // normalization of the inverse FFT output
    std::cout << "Normalizing output sequentially...\n";
    auto t_norm = std::chrono::high_resolution_clock::now();
    normalize_output_sequential(revR, revG, revB, npix);
    auto t_norm_end = std::chrono::high_resolution_clock::now();
    timing.norm_time = (t_norm_end - t_norm).count();

    // convert back to uint8 image format
    std::cout << "Converting output back to 8-bit RGB...\n";
    auto t_conv = std::chrono::high_resolution_clock::now();

    unsigned char* output_img = new unsigned char[npix * 3];
    for (int i = 0; i < npix; i++) {
        double r = std::round(revR[i][0]);
        double g = std::round(revG[i][0]);
        double b = std::round(revB[i][0]);
        
        r = std::max(0.0, std::min(r, 255.0));
        g = std::max(0.0, std::min(g, 255.0));
        b = std::max(0.0, std::min(b, 255.0));
        
        output_img[i * 3 + 0] = static_cast<unsigned char>(r);
        output_img[i * 3 + 1] = static_cast<unsigned char>(g);
        output_img[i * 3 + 2] = static_cast<unsigned char>(b);
    }
    
    auto t_conv_end = std::chrono::high_resolution_clock::now();
    timing.conv_time = (t_conv_end - t_conv).count();

    // write output image
    write_output_image(OUTPUT_PATH, output_img, width, height);

    // cleanup
    delete[] output_img;
    
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

    // results
    auto t_end = std::chrono::high_resolution_clock::now();
    timing.total_time = (t_end - t_start).count();
    
    timing.print_summary("FULLY SEQUENTIAL");
    
    return 0;
}