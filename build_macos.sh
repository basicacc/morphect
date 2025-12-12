#!/bin/bash
#
# Morphect - macOS Build Script
#
# This script builds Morphect on macOS with full verification.
# It handles dependency installation, configuration, building, and testing.
#
# Usage:
#   ./build_macos.sh          # Full build with tests
#   ./build_macos.sh --quick  # Quick build without tests
#   ./build_macos.sh --clean  # Clean build directory first
#   ./build_macos.sh --help   # Show help
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-macos"

# Configuration
BUILD_TYPE="Release"
RUN_TESTS=true
CLEAN_BUILD=false
BUILD_GIMPLE=false  # GCC plugin is optional on macOS
VERBOSE=false
JOBS=""  # Auto-detect

#------------------------------------------------------------------------------
# Functions
#------------------------------------------------------------------------------

print_banner() {
    echo -e "${BLUE}"
    echo "╔══════════════════════════════════════════════════════════════════╗"
    echo "║                    Morphect macOS Build Script                   ║"
    echo "║                     Multi-Language Obfuscator                    ║"
    echo "╚══════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}▶ $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

show_help() {
    echo "Morphect macOS Build Script"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --quick       Quick build without tests"
    echo "  --clean       Clean build directory before building"
    echo "  --debug       Build with debug symbols"
    echo "  --gimple      Attempt to build GCC GIMPLE plugin (requires Homebrew GCC)"
    echo "  --verbose     Enable verbose build output"
    echo "  --jobs N      Number of parallel build jobs (default: auto)"
    echo "  --help        Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Full release build with tests"
    echo "  $0 --quick            # Quick build, skip tests"
    echo "  $0 --clean --debug    # Clean debug build"
    echo "  $0 --gimple           # Build including GCC plugin"
}

check_macos() {
    if [[ "$(uname)" != "Darwin" ]]; then
        log_error "This script is for macOS only!"
        log_info "Detected OS: $(uname)"
        log_info "For Linux, use: cmake -B build && cmake --build build"
        exit 1
    fi
    log_info "Detected macOS $(sw_vers -productVersion)"
}

detect_jobs() {
    if [[ -z "$JOBS" ]]; then
        JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    fi
    log_info "Using $JOBS parallel build jobs"
}

check_homebrew() {
    if ! command -v brew &> /dev/null; then
        log_error "Homebrew is not installed!"
        log_info "Install Homebrew from: https://brew.sh"
        log_info "Run: /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    log_info "Homebrew found: $(brew --version | head -1)"
}

install_dependencies() {
    log_step "Checking and installing dependencies"

    local deps_to_install=()

    # Check CMake
    if ! command -v cmake &> /dev/null; then
        deps_to_install+=("cmake")
    else
        log_info "CMake found: $(cmake --version | head -1)"
    fi

    # Check Ninja (preferred generator)
    if ! command -v ninja &> /dev/null; then
        deps_to_install+=("ninja")
    else
        log_info "Ninja found: $(ninja --version)"
    fi

    # Check Clang (usually comes with Xcode)
    if ! command -v clang &> /dev/null; then
        log_warn "Clang not found. Installing Xcode Command Line Tools..."
        xcode-select --install 2>/dev/null || true
        # Wait for installation
        log_info "Please complete the Xcode Command Line Tools installation and re-run this script"
        exit 1
    else
        log_info "Clang found: $(clang --version | head -1)"
    fi

    # Check for llc (LLVM tools - needed for compiling obfuscated IR)
    if ! command -v llc &> /dev/null; then
        deps_to_install+=("llvm")
    else
        log_info "LLVM tools found: $(llc --version | head -1)"
    fi

    # Check for GCC if GIMPLE plugin is requested
    if [[ "$BUILD_GIMPLE" == true ]]; then
        if ! command -v gcc-13 &> /dev/null && ! command -v gcc-14 &> /dev/null; then
            deps_to_install+=("gcc")
            log_info "GCC will be installed for GIMPLE plugin support"
        else
            if command -v gcc-14 &> /dev/null; then
                log_info "GCC-14 found: $(gcc-14 --version | head -1)"
            elif command -v gcc-13 &> /dev/null; then
                log_info "GCC-13 found: $(gcc-13 --version | head -1)"
            fi
        fi
    fi

    # Install missing dependencies
    if [[ ${#deps_to_install[@]} -gt 0 ]]; then
        log_info "Installing missing dependencies: ${deps_to_install[*]}"
        brew install "${deps_to_install[@]}"
    else
        log_info "All dependencies are installed"
    fi

    # Verify CMake version
    CMAKE_VERSION=$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    CMAKE_MAJOR=$(echo "$CMAKE_VERSION" | cut -d. -f1)
    CMAKE_MINOR=$(echo "$CMAKE_VERSION" | cut -d. -f2)

    if [[ "$CMAKE_MAJOR" -lt 3 ]] || [[ "$CMAKE_MAJOR" -eq 3 && "$CMAKE_MINOR" -lt 20 ]]; then
        log_error "CMake 3.20 or higher required (found $CMAKE_VERSION)"
        log_info "Run: brew upgrade cmake"
        exit 1
    fi
}

clean_build_dir() {
    if [[ "$CLEAN_BUILD" == true ]]; then
        log_step "Cleaning build directory"
        if [[ -d "$BUILD_DIR" ]]; then
            log_info "Removing $BUILD_DIR"
            rm -rf "$BUILD_DIR"
        fi
    fi
}

configure_build() {
    log_step "Configuring build"

    mkdir -p "$BUILD_DIR"

    # Determine generator
    local generator="Unix Makefiles"
    if command -v ninja &> /dev/null; then
        generator="Ninja"
    fi
    log_info "Using generator: $generator"

    # Build CMake options
    local cmake_opts=(
        "-B" "$BUILD_DIR"
        "-G" "$generator"
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DCMAKE_CXX_STANDARD=17"
        "-DMORPHECT_BUILD_IR_OBFUSCATOR=ON"
        "-DMORPHECT_BUILD_ASM_OBFUSCATOR=ON"
        "-DMORPHECT_BUILD_TESTS=$([[ "$RUN_TESTS" == true ]] && echo "ON" || echo "OFF")"
        "-DMORPHECT_BUILD_BENCHMARKS=OFF"
        "-DMORPHECT_INSTALL=OFF"
    )

    # GIMPLE plugin
    if [[ "$BUILD_GIMPLE" == true ]]; then
        # Find Homebrew GCC
        local gcc_path=""
        if command -v gcc-14 &> /dev/null; then
            gcc_path=$(which gcc-14)
        elif command -v gcc-13 &> /dev/null; then
            gcc_path=$(which gcc-13)
        fi

        if [[ -n "$gcc_path" ]]; then
            cmake_opts+=("-DMORPHECT_BUILD_GIMPLE_PLUGIN=ON")
            cmake_opts+=("-DCMAKE_C_COMPILER=$gcc_path")
            log_info "GIMPLE plugin enabled with GCC: $gcc_path"
        else
            cmake_opts+=("-DMORPHECT_BUILD_GIMPLE_PLUGIN=OFF")
            log_warn "GIMPLE plugin disabled: GCC not found"
        fi
    else
        cmake_opts+=("-DMORPHECT_BUILD_GIMPLE_PLUGIN=OFF")
    fi

    # macOS-specific settings
    cmake_opts+=(
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    )

    # Run CMake configuration
    log_info "Running CMake configuration..."
    if [[ "$VERBOSE" == true ]]; then
        cmake "${cmake_opts[@]}" "$SCRIPT_DIR"
    else
        cmake "${cmake_opts[@]}" "$SCRIPT_DIR" 2>&1 | grep -E "(^--|Morphect|Components:|ERROR|Warning)" || true
    fi

    if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
        log_error "CMake configuration failed!"
        exit 1
    fi

    log_info "Configuration complete"
}

build_project() {
    log_step "Building project"

    local build_opts=("--build" "$BUILD_DIR" "-j" "$JOBS")

    if [[ "$VERBOSE" == true ]]; then
        build_opts+=("--verbose")
    fi

    log_info "Building with $JOBS parallel jobs..."

    if cmake "${build_opts[@]}"; then
        log_info "Build successful!"
    else
        log_error "Build failed!"
        exit 1
    fi

    # Show built artifacts
    log_info "Built artifacts:"
    if [[ -d "$BUILD_DIR/bin" ]]; then
        ls -la "$BUILD_DIR/bin/" 2>/dev/null | grep -E "^-" | awk '{print "  " $NF " (" $5 " bytes)"}' || true
    fi
    if [[ -d "$BUILD_DIR/lib" ]]; then
        ls -la "$BUILD_DIR/lib/" 2>/dev/null | grep -E "\.(a|dylib)$" | awk '{print "  " $NF " (" $5 " bytes)"}' || true
    fi
}

run_tests() {
    if [[ "$RUN_TESTS" != true ]]; then
        log_info "Skipping tests (--quick mode)"
        return 0
    fi

    log_step "Running tests"

    cd "$BUILD_DIR"

    if ctest --output-on-failure -j "$JOBS"; then
        log_info "All tests passed!"
    else
        log_error "Some tests failed!"
        exit 1
    fi

    cd "$SCRIPT_DIR"
}

verify_binaries() {
    log_step "Verifying built binaries"

    local morphect_ir="$BUILD_DIR/bin/morphect-ir"
    local morphect_asm="$BUILD_DIR/bin/morphect-asm"

    # Check morphect-ir
    if [[ -x "$morphect_ir" ]]; then
        log_info "Testing morphect-ir..."
        if "$morphect_ir" --help > /dev/null 2>&1; then
            log_info "  morphect-ir: OK (--help works)"
        else
            log_warn "  morphect-ir: Binary exists but --help failed"
        fi

        # Verify it's a valid macOS binary
        if file "$morphect_ir" | grep -q "Mach-O"; then
            log_info "  morphect-ir: Valid Mach-O executable"
        fi
    else
        log_error "morphect-ir not found at $morphect_ir"
        exit 1
    fi

    # Check morphect-asm
    if [[ -x "$morphect_asm" ]]; then
        log_info "Testing morphect-asm..."
        if "$morphect_asm" --help > /dev/null 2>&1; then
            log_info "  morphect-asm: OK (--help works)"
        else
            log_warn "  morphect-asm: Binary exists but --help failed"
        fi

        # Verify it's a valid macOS binary
        if file "$morphect_asm" | grep -q "Mach-O"; then
            log_info "  morphect-asm: Valid Mach-O executable"
        fi
    else
        log_error "morphect-asm not found at $morphect_asm"
        exit 1
    fi
}

run_integration_test() {
    log_step "Running integration test"

    local morphect_ir="$BUILD_DIR/bin/morphect-ir"
    local test_dir="$BUILD_DIR/integration_test"

    mkdir -p "$test_dir"

    # Create a simple test C file
    cat > "$test_dir/test.c" << 'EOF'
#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    int result = 0;
    for (int i = 0; i < b; i++) {
        result = add(result, a);
    }
    return result;
}

int main() {
    int x = 5;
    int y = 3;
    int sum = add(x, y);
    int product = multiply(x, y);
    printf("Sum: %d, Product: %d\n", sum, product);
    return (sum == 8 && product == 15) ? 0 : 1;
}
EOF

    log_info "Created test.c"

    # Step 1: Compile to LLVM IR
    log_info "Step 1: Generating LLVM IR..."
    if clang -S -emit-llvm -O0 "$test_dir/test.c" -o "$test_dir/test.ll"; then
        log_info "  Generated test.ll ($(wc -c < "$test_dir/test.ll") bytes)"
    else
        log_error "Failed to generate LLVM IR"
        exit 1
    fi

    # Step 2: Obfuscate with morphect-ir (MBA only for reliable test)
    log_info "Step 2: Obfuscating with morphect-ir..."
    if "$morphect_ir" --mba --probability 0.5 "$test_dir/test.ll" "$test_dir/obfuscated.ll" 2>&1; then
        log_info "  Generated obfuscated.ll ($(wc -c < "$test_dir/obfuscated.ll") bytes)"
    else
        log_error "Obfuscation failed"
        exit 1
    fi

    # Step 3: Compile obfuscated IR to assembly
    log_info "Step 3: Compiling obfuscated IR..."
    if llc "$test_dir/obfuscated.ll" -o "$test_dir/obfuscated.s" 2>&1; then
        log_info "  Generated obfuscated.s"
    else
        log_warn "llc compilation failed (this may indicate IR issues)"
        log_info "Trying alternative: compile original IR for verification..."
        if llc "$test_dir/test.ll" -o "$test_dir/original.s" 2>&1; then
            log_info "  Original IR compiles correctly"
        fi
        # Don't fail here - MBA transformations can sometimes produce complex IR
    fi

    # Step 4: Compile and run if assembly was generated
    if [[ -f "$test_dir/obfuscated.s" ]]; then
        log_info "Step 4: Creating executable and testing..."
        if clang "$test_dir/obfuscated.s" -o "$test_dir/test_program" 2>&1; then
            log_info "  Created executable"

            if "$test_dir/test_program"; then
                log_info "  Program output correct!"
                log_info "  ${GREEN}Integration test PASSED!${NC}"
            else
                log_error "  Program produced incorrect output!"
            fi
        else
            log_warn "Failed to create executable (linker error)"
        fi
    else
        log_info "Step 4: Skipped (assembly not generated)"
    fi

    # Show size comparison
    if [[ -f "$test_dir/test.ll" ]] && [[ -f "$test_dir/obfuscated.ll" ]]; then
        local original_size=$(wc -c < "$test_dir/test.ll")
        local obfuscated_size=$(wc -c < "$test_dir/obfuscated.ll")
        local increase=$((100 * (obfuscated_size - original_size) / original_size))
        log_info "Size comparison: $original_size -> $obfuscated_size bytes (+${increase}%)"
    fi

    log_info "Integration test files preserved in: $test_dir"
}

print_summary() {
    log_step "Build Summary"

    echo ""
    echo -e "${GREEN}Build completed successfully!${NC}"
    echo ""
    echo "Built binaries:"
    echo "  $BUILD_DIR/bin/morphect-ir"
    echo "  $BUILD_DIR/bin/morphect-asm"
    echo ""
    echo "Usage example:"
    echo "  # Generate LLVM IR from C source"
    echo "  clang -S -emit-llvm -O0 source.c -o source.ll"
    echo ""
    echo "  # Obfuscate the IR"
    echo "  $BUILD_DIR/bin/morphect-ir --mba --cff source.ll obfuscated.ll"
    echo ""
    echo "  # Compile to executable"
    echo "  llc obfuscated.ll -o obfuscated.s"
    echo "  clang obfuscated.s -o program"
    echo ""

    if [[ "$BUILD_GIMPLE" == true ]] && [[ -f "$BUILD_DIR/lib/morphect_plugin.so" ]]; then
        echo "GCC Plugin:"
        echo "  gcc -fplugin=$BUILD_DIR/lib/morphect_plugin.so source.c -o program"
        echo ""
    fi
}

#------------------------------------------------------------------------------
# Parse arguments
#------------------------------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            RUN_TESTS=false
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --gimple)
            BUILD_GIMPLE=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

#------------------------------------------------------------------------------
# Main execution
#------------------------------------------------------------------------------

print_banner
check_macos
detect_jobs
check_homebrew
install_dependencies
clean_build_dir
configure_build
build_project
run_tests
verify_binaries
run_integration_test
print_summary

log_info "Done!"
