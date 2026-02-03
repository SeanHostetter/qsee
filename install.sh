#!/bin/bash

# =============================================================================
# qsee Installer Script
# =============================================================================
# This script installs qsee - a ChronusQ input file visualizer
#
# What it does:
#   1. Installs fzf (fuzzy finder) using your system's package manager
#   2. Compiles the qsee binary
#   3. Installs files to ~/bin/qsee_bin/
#   4. Installs the qsee wrapper script to ~/.local/bin/
# =============================================================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}"
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                    qsee Installer                             ║"
echo "║         ChronusQ Input File Visualizer                        ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo -e "${NC}"

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Installation directories
BIN_DIR="$HOME/bin/qsee_bin"
SCRIPT_INSTALL_DIR="$HOME/.local/bin"

# =============================================================================
# Step 1: Detect package manager and install fzf
# =============================================================================
echo -e "${YELLOW}[1/4] Checking for fzf...${NC}"

if command -v fzf &> /dev/null; then
    echo -e "${GREEN}  ✓ fzf is already installed${NC}"
else
    echo -e "  Installing fzf..."
    
    if command -v pacman &> /dev/null; then
        # Arch Linux
        echo -e "  Detected: ${CYAN}Arch Linux${NC} (pacman)"
        sudo pacman -S --noconfirm fzf
    elif command -v apt &> /dev/null; then
        # Debian/Ubuntu
        echo -e "  Detected: ${CYAN}Debian/Ubuntu${NC} (apt)"
        sudo apt update && sudo apt install -y fzf
    elif command -v dnf &> /dev/null; then
        # Fedora
        echo -e "  Detected: ${CYAN}Fedora${NC} (dnf)"
        sudo dnf install -y fzf
    elif command -v brew &> /dev/null; then
        # macOS (Homebrew)
        echo -e "  Detected: ${CYAN}macOS${NC} (Homebrew)"
        brew install fzf
    elif command -v zypper &> /dev/null; then
        # openSUSE
        echo -e "  Detected: ${CYAN}openSUSE${NC} (zypper)"
        sudo zypper install -y fzf
    else
        echo -e "${RED}  ✗ Could not detect package manager.${NC}"
        echo -e "  Please install fzf manually: https://github.com/junegunn/fzf"
        exit 1
    fi
    
    echo -e "${GREEN}  ✓ fzf installed successfully${NC}"
fi

# =============================================================================
# Step 2: Check for g++ and compile the binary
# =============================================================================
echo -e "${YELLOW}[2/4] Compiling qsee...${NC}"

if ! command -v g++ &> /dev/null; then
    echo -e "${RED}  ✗ g++ not found. Please install a C++ compiler.${NC}"
    
    if command -v pacman &> /dev/null; then
        echo -e "  Try: ${CYAN}sudo pacman -S gcc${NC}"
    elif command -v apt &> /dev/null; then
        echo -e "  Try: ${CYAN}sudo apt install build-essential${NC}"
    fi
    exit 1
fi

cd "$SCRIPT_DIR"

# Compile the binary
g++ -std=c++17 -O2 -o qsee_exe qsee.cpp Input.cpp -lm

if [[ -f "qsee_exe" ]]; then
    echo -e "${GREEN}  ✓ Compiled successfully${NC}"
else
    echo -e "${RED}  ✗ Compilation failed${NC}"
    exit 1
fi

# =============================================================================
# Step 3: Install binary and supporting files to ~/bin/qsee_bin/
# =============================================================================
echo -e "${YELLOW}[3/4] Installing to $BIN_DIR...${NC}"

mkdir -p "$BIN_DIR"

# Copy the compiled binary
cp qsee_exe "$BIN_DIR/"

# Copy source files (optional, for reference/recompilation)
cp qsee.cpp Input.cpp Input.hpp "$BIN_DIR/" 2>/dev/null || true

echo -e "${GREEN}  ✓ Files installed to $BIN_DIR${NC}"

# =============================================================================
# Step 4: Install wrapper script to ~/.local/bin/
# =============================================================================
echo -e "${YELLOW}[4/4] Installing qsee script to $SCRIPT_INSTALL_DIR...${NC}"

mkdir -p "$SCRIPT_INSTALL_DIR"

# Copy the wrapper script
cp qsee "$SCRIPT_INSTALL_DIR/qsee"
chmod +x "$SCRIPT_INSTALL_DIR/qsee"

echo -e "${GREEN}  ✓ qsee script installed to $SCRIPT_INSTALL_DIR${NC}"

# =============================================================================
# Post-installation: Check if ~/.local/bin is in PATH
# =============================================================================
echo ""
if [[ ":$PATH:" != *":$SCRIPT_INSTALL_DIR:"* ]]; then
    echo -e "${YELLOW}⚠ Note: $SCRIPT_INSTALL_DIR is not in your PATH.${NC}"
    echo -e "  Add this line to your ~/.bashrc or ~/.zshrc:"
    echo -e "  ${CYAN}export PATH=\"\$HOME/.local/bin:\$PATH\"${NC}"
    echo ""
fi

# =============================================================================
# Done!
# =============================================================================
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗"
echo -e "║               Installation Complete! 🎉                        ║"
echo -e "╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Usage:"
echo -e "  ${CYAN}qsee <input_file.inp>${NC}     - Visualize a specific file"
echo -e "  ${CYAN}qsee${NC}                      - Interactive file picker (uses fzf)"
echo -e "  ${CYAN}qsee -xy${NC}                  - View XY plane"
echo ""
echo -e "Enjoy! 🧪"
