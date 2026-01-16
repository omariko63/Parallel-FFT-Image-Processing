cmake_minimum_required(VERSION 3.12)
project(ParallelFFTImageProcessing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# cross-platform build confi
if(WIN32)
    message(STATUS "Building for Windows")
elseif(APPLE)
    message(STATUS "Building for macOS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -ffast-math")
else()
    message(STATUS "Building for Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -ffast-math")
endif()

# OpenMP cross-platform detection
if(APPLE)
    # macOS: Try to find Homebrew OpenMP first
    find_library(OMP_LIB NAMES omp PATHS /opt/homebrew/opt/libomp/lib NO_DEFAULT_PATH)
    
    if(OMP_LIB)
        message(STATUS "Found Homebrew OpenMP at ${OMP_LIB}")
        
        # Set OpenMP manually for Homebrew
        set(OpenMP_CXX_FLAGS "-Xclang -fopenmp")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "${OMP_LIB}")
        set(OPENMP_FOUND TRUE)
        set(OpenMP_CXX_FOUND TRUE)
        set(OPENMP_AVAILABLE TRUE)
        
        # Add include and lib paths
        include_directories(/opt/homebrew/opt/libomp/include)
        link_directories(/opt/homebrew/opt/libomp/lib)
        
    else()
        message(STATUS "WARNING:  OpenMP (libomp) not found on macOS")
        message(STATUS "")
        message(STATUS "To install OpenMP:")
        message(STATUS "  1. brew install libomp")
        message(STATUS "  2. cd .. && rm -rf build && mkdir build && cd build")
        message(STATUS "  3. cmake -DCMAKE_BUILD_TYPE=Release ..")
        message(STATUS "")
        
        set(OPENMP_AVAILABLE FALSE)
        message(WARNING "Building WITHOUT OpenMP support. Parallelization disabled.")
    endif()
else()
    # Linux/Windows: OpenMP is required
    find_package(OpenMP REQUIRED)
    set(OPENMP_AVAILABLE TRUE)
    message(STATUS "OpenMP found")
endif()

# FFTW3 cross-platform detection
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(FFTW fftw3 fftw3_omp)
endif()

if(NOT FFTW_FOUND)
    find_path(FFTW_INCLUDE_DIR NAMES fftw3.h)
    find_library(FFTW_LIBRARY NAMES fftw3 libfftw3)
    find_library(FFTW_OMP_LIBRARY NAMES fftw3_omp libfftw3_omp)
    
    if(FFTW_INCLUDE_DIR AND FFTW_LIBRARY)
        set(FFTW_FOUND TRUE)
        set(FFTW_INCLUDE_DIRS ${FFTW_INCLUDE_DIR})
        set(FFTW_LIBRARIES ${FFTW_LIBRARY})
        if(FFTW_OMP_LIBRARY)
            list(APPEND FFTW_LIBRARIES ${FFTW_OMP_LIBRARY})
        endif()
        message(STATUS "SUCCESS: FFTW3 found manually")
    else()
        message(FATAL_ERROR 
            "ERROR: FFTW3 not found!\n"
            "Install with:\n"
            "  macOS: brew install fftw\n"
            "  Ubuntu/Debian: sudo apt-get install libfftw3-dev libfftw3-openmp-dev\n"
            "  Windows: Download from http://www.fftw.org/install/windows.html"
        )
    endif()
endif()

message(STATUS "FFTW Include: ${FFTW_INCLUDE_DIRS}")
message(STATUS "FFTW Libraries: ${FFTW_LIBRARIES}")

include_directories(${FFTW_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# common code
add_library(fft_common OBJECT
    src/stb_impl.cpp
)

target_include_directories(fft_common PUBLIC include)

# sequential version
add_executable(fft_processor_fully_sequential
    src/main_fully_sequential.cpp
    $<TARGET_OBJECTS:fft_common>
)

target_include_directories(fft_processor_fully_sequential PRIVATE include)

target_link_libraries(fft_processor_fully_sequential 
    ${FFTW_LIBRARY}
    m
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fft_processor_fully_sequential PRIVATE -O3)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(fft_processor_fully_sequential PRIVATE /O2)
endif()

# partially parallel version
add_executable(fft_processor_partially_parallel
    src/main_partially_parallel.cpp
    $<TARGET_OBJECTS:fft_common>
)

target_include_directories(fft_processor_partially_parallel PRIVATE include)

target_link_libraries(fft_processor_partially_parallel
    ${FFTW_LIBRARY}
    m
)

if(OPENMP_AVAILABLE)
    target_link_libraries(fft_processor_partially_parallel omp)
    if(APPLE)
        target_compile_options(fft_processor_partially_parallel PRIVATE
            -O3 -march=native -Xclang -fopenmp
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(fft_processor_partially_parallel PRIVATE
            -O3 -march=native -fopenmp
        )

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(fft_processor_partially_parallel PRIVATE /O2 /openmp)
    endif()
else()
    message(STATUS "WARNING:  Building partially_parallel WITHOUT OpenMP")
endif()

# parallel version
add_executable(fft_processor_fully_parallel
    src/main_fully_parallel.cpp
    $<TARGET_OBJECTS:fft_common>
)

target_include_directories(fft_processor_fully_parallel PRIVATE include)

target_link_libraries(fft_processor_fully_parallel
    ${FFTW_LIBRARIES}
    m
)

if(OPENMP_AVAILABLE)
    target_link_libraries(fft_processor_fully_parallel omp)
    if(APPLE)
        target_compile_options(fft_processor_fully_parallel PRIVATE
            -O3 -march=native -Xclang -fopenmp
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(fft_processor_fully_parallel PRIVATE
            -O3 -march=native -mtune=native -ffast-math -flto -fopenmp
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(fft_processor_fully_parallel PRIVATE /O2 /fp:fast /openmp)
        target_link_options(fft_processor_fully_parallel PRIVATE /LTCG)
    endif()
else()
    message(STATUS "Building fully_parallel WITHOUT OpenMP")
endif()


# ============================================================================
# OUTPUT DIRECTORIES & BENCHMARK RUNNER
# ============================================================================

set_target_properties(
    fft_processor_fully_sequential 
    fft_processor_partially_parallel 
    fft_processor_fully_parallel 
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# BUILD SUMMARY
# ============================================================================

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════╗")
message(STATUS "║  FFT Image Processing - 3-Version Build Configuration      ║")
message(STATUS "╚════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenMP: ${OPENMP_AVAILABLE}")
message(STATUS "")
message(STATUS "Building 3 versions for performance comparison:")
message(STATUS "")
message(STATUS "  1  fft_processor_fully_sequential")
message(STATUS "     - No #pragma omp (completely sequential)")
message(STATUS "     - FFTW_ESTIMATE planning")
message(STATUS "     - Expected: ~400-450ms (SLOWEST)")
message(STATUS "")
message(STATUS "  2  fft_processor_partially_parallel")
if(OPENMP_AVAILABLE)
    message(STATUS "     - Loops parallelized with OpenMP")
    message(STATUS "     - FFTs still sequential (R→G→B)")
    message(STATUS "     - Expected: ~280-300ms (MEDIUM)")
else()
    message(STATUS "     - ⚠️  Built WITHOUT OpenMP (will run sequentially)")
    message(STATUS "     - Install OpenMP for parallelization")
endif()
message(STATUS "")
message(STATUS "  3  fft_processor_fully_parallel")
if(OPENMP_AVAILABLE)
    message(STATUS "     - Everything parallelized (loops + FFT sections)")
    message(STATUS "     - FFTW threading + OpenMP")
    message(STATUS "     - FFTW_MEASURE planning")
    message(STATUS "     - Expected: ~140ms (FASTEST - 3x speedup!)")
else()
    message(STATUS "     - ⚠️  Built WITHOUT OpenMP (will run sequentially)")
    message(STATUS "     - Install OpenMP for full optimization")
endif()
message(STATUS "")
message(STATUS "Build commands:")
message(STATUS "  cmake -DCMAKE_BUILD_TYPE=Release ..")
message(STATUS "  cmake --build . --parallel")
message(STATUS "")
message(STATUS "Run all benchmarks:")
if(WIN32)
    message(STATUS "  ./run_benchmark.bat")
else()
    message(STATUS "  bash ./run_benchmark.sh")
endif()
message(STATUS "")

include(${CMAKE_SOURCE_DIR}/cmake_benchmark_script.cmake)