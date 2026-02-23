#!/bin/bash

# --- Color Definitions ---
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BLUE='\033[34m'
CYAN='\033[36m'
BOLD='\033[1m'
RESET='\033[0m'

SEARCH_DIRS="backend/include interp/include middleend/include modules"

echo -e "${CYAN}${BOLD}Building Ry...${RESET}"

# Create folders
echo -e "\nCreating folders..."
mkdir -p build
mkdir -p lib
mkdir -p bin

if [ "$1" == "-G" ]; then
    # Create build files
    echo -e "\nCreating build files with generator $2 ..."
    cd build
    if [ "$2" != "" ]; then
        cmake .. -G "$2"
    else
        echo -e "${YELLOW}${BOLD}Pls enter a generator.${RESET}"
        exit 1
    fi

    cd ../
fi

if [ -z "$(find "build" -maxdepth 1 -not -type d -print -quit 2>/dev/null)" ]; then
    echo -e "\nBuilding with default generator"
    cd build
    cmake ..
    cd ../
fi

# Execute Build
cmake --build build -j1

# Success Check 
if [ $? -eq 0 ]; then
    cp build/ry bin
    cp build/*.so lib
    echo -e "\n${GREEN}${BOLD}Build complete.${RESET}"
else
    echo -e "\n${RED}${BOLD}Build failed.${RESET} Check the errors above."
    exit 1
fi