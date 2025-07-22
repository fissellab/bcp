#!/bin/bash

# Automatic dependency installer for BCP Saggitarius on Ubuntu/Debian
echo "=== BCP Saggitarius Dependency Installer ==="
echo "This script will install all required dependencies for Ubuntu/Debian systems."
echo ""

# Function to check if running on Ubuntu/Debian
check_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        if [[ "$ID" == "ubuntu" ]] || [[ "$ID" == "debian" ]] || [[ "$ID_LIKE" == *"ubuntu"* ]] || [[ "$ID_LIKE" == *"debian"* ]]; then
            echo "✅ Detected $PRETTY_NAME"
            return 0
        fi
    fi
    echo "❌ This script is designed for Ubuntu/Debian systems only."
    echo "   Please install dependencies manually using your system's package manager."
    exit 1
}

# Check OS compatibility
check_os

# Ask for confirmation
read -p "Do you want to proceed with automatic installation? (y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Installation cancelled."
    exit 0
fi

echo "Installing system dependencies..."

# Update package list
echo "Updating package list..."
sudo apt-get update

# Install basic dependencies
echo "Installing Git, GCC, and json-c..."
sudo apt-get install -y git gcc libjson-c-dev pkg-config

# Install CMake 3.24+
echo "Installing CMake 3.24+..."
if ! command -v cmake >/dev/null 2>&1; then
    # Add Kitware's repository for latest CMake
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
    
    # Determine Ubuntu/Debian version
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        if [[ "$VERSION_CODENAME" ]]; then
            CODENAME="$VERSION_CODENAME"
        else
            # Fallback for older systems
            CODENAME="focal"
        fi
    else
        CODENAME="focal"
    fi
    
    sudo apt-add-repository "deb https://apt.kitware.com/ubuntu/ $CODENAME main"
    sudo apt-get update
    sudo apt-get install -y cmake
else
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
    CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
    
    if [ "$CMAKE_MAJOR" -lt 3 ] || [ "$CMAKE_MAJOR" -eq 3 -a "$CMAKE_MINOR" -lt 24 ]; then
        echo "Upgrading CMake to 3.24+..."
        sudo apt-get install -y cmake
    else
        echo "CMake $CMAKE_VERSION is already up to date."
    fi
fi

# Install vcpkg
echo "Installing vcpkg..."
VCPKG_DIR="$HOME/vcpkg"

if [ ! -d "$VCPKG_DIR" ]; then
    git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_DIR"
    cd "$VCPKG_DIR"
    ./bootstrap-vcpkg.sh
    cd - >/dev/null
    
    # Add VCPKG_ROOT to bashrc if not already present
    if ! grep -q "VCPKG_ROOT" ~/.bashrc; then
        echo "" >> ~/.bashrc
        echo "# vcpkg configuration" >> ~/.bashrc
        echo "export VCPKG_ROOT=$VCPKG_DIR" >> ~/.bashrc
        echo "export PATH=\$VCPKG_ROOT:\$PATH" >> ~/.bashrc
    fi
    
    echo "✅ vcpkg installed to $VCPKG_DIR"
    echo "   VCPKG_ROOT environment variable added to ~/.bashrc"
else
    echo "✅ vcpkg already exists at $VCPKG_DIR"
fi

echo ""
echo "🎉 All dependencies installed successfully!"
echo ""
echo "⚠️  IMPORTANT: Please run the following command to reload your environment:"
echo "   source ~/.bashrc"
echo ""
echo "Then you can build the project with:"
echo "   ./build_and_run.sh"
echo "" 