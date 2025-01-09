#!/bin/sh

REPO_DIR="/home/riko125/oslabs/myproj"
BUILD_DIR="/home/riko125/oslabs/myproj"

cd "$REPO_DIR" || exit

echo "Updating repository..."
git pull origin main

if [ $? -ne 0 ]; then
    echo "Error updating repository. Exiting."
    exit 1
fi


# Сборка и компиляция проекта
echo "Building project..."
cd "$BUILD_DIR"
cmake "$REPO_DIR" .

if [ $? -ne 0 ]; then
    echo "Build failed. Exiting."
    exit 1
fi

echo "Build completed successfully."
exit 0
