#!/bin/bash

# Define paths relative to the script location
RY_ROOT=$(dirname "$(readlink -f "$0")")/..
DEST_DIR="/usr/share/ry"
LIB_DIR="/usr/lib/ry"


echo -e "\e[34mInstalling Ry...\e[0m"

# Create system folders
sudo mkdir -p $DEST_DIR/bin $DEST_DIR/lib
sudo mkdir -p $LIB_DIR

# Copy from your project's build folders
sudo cp $RY_ROOT/bin/ry $DEST_DIR/bin/
sudo cp $RY_ROOT/lib/*.a $DEST_DIR/lib/ 2>/dev/null || true

# Copy standard library
sudo cp $RY_ROOT/lib/*.so $LIB_DIR/ 2>/dev/null || true
sudo cp $RY_ROOT/modules/library* $LIB_DIR/ 2>/dev/null || true

# Set permissions and symlink
sudo chmod +x $DEST_DIR/bin/ry
sudo ln -sf $DEST_DIR/bin/ry /usr/local/bin/ry

# Update library paths
echo "$DEST_DIR/lib" | sudo tee /etc/ld.so.conf.d/ry.conf > /dev/null
sudo ldconfig

echo -e "\e[32mRy is now installed system-wide!\e[0m"