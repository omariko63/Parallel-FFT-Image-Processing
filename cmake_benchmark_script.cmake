# ============================================================================
# FFT IMAGE PROCESSING - BENCHMARK SCRIPT GENERATION
# ============================================================================

if(NOT WIN32)
    # Create bash script with timing capture and speedup calculation
    set(BENCHMARK_SCRIPT "#!/bin/bash

# FFT Image Processing Benchmark Script
# Runs all three versions, captures timing, and calculates speedups
# Usage: bash run_benchmark.sh

set -e  # Exit on error

SCRIPT_DIR=\"\$(cd \"\$(dirname \"\${BASH_SOURCE[0]}\")\" && pwd)\"
BIN_DIR=\"\${SCRIPT_DIR}/bin\"

# Color codes for output
RED='\\033[0;31m'
GREEN='\\033[0;32m'
YELLOW='\\033[1;33m'
BLUE='\\033[0;34m'
NC='\\033[0m' # No color

# Header
clear
echo -e \"\${YELLOW}╔════════════════════════════════════════════════════════════╗\${NC}\"
echo -e \"\${YELLOW}║  FFT Image Processing - 3-Version Benchmark                ║\${NC}\"
echo -e \"\${YELLOW}║  Fully Sequential vs Partially Parallel vs Fully Parallel  ║\${NC}\"
echo -e \"\${YELLOW}╚════════════════════════════════════════════════════════════╝\${NC}\"
echo \"\"

# Check if binaries exist
if [ ! -f \"\${BIN_DIR}/fft_processor_fully_sequential\" ]; then
    echo -e \"\${RED}Error: fft_processor_fully_sequential not found\${NC}\"
    echo \"Please build first with: cmake --build . --parallel\"
    exit 1
fi

if [ ! -f \"\${BIN_DIR}/fft_processor_partially_parallel\" ]; then
    echo -e \"\${RED}Error: fft_processor_partially_parallel not found\${NC}\"
    echo \"Please build first with: cmake --build . --parallel\"
    exit 1
fi

if [ ! -f \"\${BIN_DIR}/fft_processor_fully_parallel\" ]; then
    echo -e \"\${RED}Error: fft_processor_fully_parallel not found\${NC}\"
    echo \"Please build first with: cmake --build . --parallel\"
    exit 1
fi

cd \"\${SCRIPT_DIR}\"

# Run benchmarks and capture total time (last line of output)
echo -e \"\${GREEN}Version 1/3: FULLY SEQUENTIAL (No parallelization)\${NC}\"
echo \"─────────────────────────────────────────────────────────────\"
SEQ_OUTPUT=\$(\"\${BIN_DIR}/fft_processor_fully_sequential\" 2>&1)
echo \"\$SEQ_OUTPUT\"
SEQ_TIME=\$(echo \"\$SEQ_OUTPUT\" | grep \"Total Time:\" | awk '{print \$3}')
echo \"\"

echo -e \"\${GREEN}Version 2/3: PARTIALLY PARALLEL (Loops parallel, FFTs sequential)\${NC}\"
echo \"─────────────────────────────────────────────────────────────\"
PARTIAL_OUTPUT=\$(\"\${BIN_DIR}/fft_processor_partially_parallel\" 2>&1)
echo \"\$PARTIAL_OUTPUT\"
PARTIAL_TIME=\$(echo \"\$PARTIAL_OUTPUT\" | grep \"Total Time:\" | awk '{print \$3}')
echo \"\"

echo -e \"\${GREEN}Version 3/3: FULLY PARALLEL (Everything parallelized)\${NC}\"
echo \"─────────────────────────────────────────────────────────────\"
FULL_OUTPUT=\$(\"\${BIN_DIR}/fft_processor_fully_parallel\" 2>&1)
echo \"\$FULL_OUTPUT\"
FULL_TIME=\$(echo \"\$FULL_OUTPUT\" | grep \"Total Time:\" | awk '{print \$3}')
echo \"\"

# Calculate speedups
SEQ_VS_PARTIAL=\$(echo \"scale=2; \$SEQ_TIME / \$PARTIAL_TIME\" | bc)
PARTIAL_VS_FULL=\$(echo \"scale=2; \$PARTIAL_TIME / \$FULL_TIME\" | bc)
SEQ_VS_FULL=\$(echo \"scale=2; \$SEQ_TIME / \$FULL_TIME\" | bc)

# Print comparison table
echo -e \"\${BLUE}╔════════════════════════════════════════════════════════╗\${NC}\"
echo -e \"\${BLUE}║           SPEEDUP COMPARISON TABLE                     ║\${NC}\"
echo -e \"\${BLUE}╠════════════════════════════════════════════════════════╣\${NC}\"
echo -e \"\${BLUE}║  Fully Sequential (baseline):        \${SEQ_TIME} s          ║\${NC}\"
echo -e \"\${BLUE}║  Partially Parallel:                 \${PARTIAL_TIME} s          ║\${NC}\"
echo -e \"\${BLUE}║  Fully Parallel:                     \${FULL_TIME} s          ║\${NC}\"
echo -e \"\${BLUE}╠════════════════════════════════════════════════════════╣\${NC}\"
echo -e \"\${GREEN}║  Sequential → Partial:  \${SEQ_VS_PARTIAL}x speedup                  ║\${NC}\"
echo -e \"\${GREEN}║  Partial → Parallel:    \${PARTIAL_VS_FULL}x speedup                  ║\${NC}\"
echo -e \"\${GREEN}║  Sequential → Parallel: \${SEQ_VS_FULL}x speedup                  ║\${NC}\"
echo -e \"\${BLUE}╚════════════════════════════════════════════════════════╝\${NC}\"
echo \"\"

# Completion message
echo -e \"\${GREEN}✓ All benchmarks complete!\${NC}\"
echo \"\"
echo \"Output images:\"
echo \"  - images/output_fully_sequential.png\"
echo \"  - images/output_partially_parallel.png\"
echo \"  - images/output_fully_parallel.png\"
")

    file(WRITE "${CMAKE_BINARY_DIR}/run_benchmark.sh" "${BENCHMARK_SCRIPT}")

    # Make the script executable
    execute_process(
        COMMAND chmod +x "${CMAKE_BINARY_DIR}/run_benchmark.sh"
        RESULT_VARIABLE chmod_result
    )

    if(NOT chmod_result EQUAL 0)
        message(WARNING "Failed to make run_benchmark.sh executable")
    else()
        message(STATUS "✓ Generated run_benchmark.sh")
    endif()

else()
    # Windows batch script with timing and speedup calculation
    file(WRITE "${CMAKE_BINARY_DIR}/run_benchmark.bat" "@echo off\nREM FFT Image Processing Benchmark Script with Speedup Analysis\nsetlocal enabledelayedexpansion\nset SCRIPT_DIR=%~dp0\nset BIN_DIR=%SCRIPT_DIR%bin\ncls\necho.\necho ╔════════════════════════════════════════════════════════════╗\necho ║  FFT Image Processing - 3-Version Benchmark                ║\necho ║  Fully Sequential vs Partially Parallel vs Fully Parallel  ║\necho ╚════════════════════════════════════════════════════════════╝\necho.\nif not exist \"%BIN_DIR%\\fft_processor_fully_sequential.exe\" (\n    echo Error: fft_processor_fully_sequential.exe not found\n    pause\n    exit /b 1\n)\necho Version 1/3: FULLY SEQUENTIAL\n\"%BIN_DIR%\\fft_processor_fully_sequential.exe\"\necho.\necho Version 2/3: PARTIALLY PARALLEL\n\"%BIN_DIR%\\fft_processor_partially_parallel.exe\"\necho.\necho Version 3/3: FULLY PARALLEL\n\"%BIN_DIR%\\fft_processor_fully_parallel.exe\"\necho.\necho All benchmarks complete!\necho.\necho Note: Windows batch doesn't easily support floating-point arithmetic.\necho Consider using PowerShell or a Python script for speedup calculations.\necho.\npause\n")
    message(STATUS "Generated run_benchmark.bat")
endif()
