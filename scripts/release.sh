#!/bin/bash

# Arguments
MSG=$1
VERSION=$2

if [ -z "$MSG" ] || [ -z "$VERSION" ]; then
    echo "Usage: ./scripts/release.sh \"Commit message\" 2.0.6"
    exit 1
fi

RELEASE_NAME="Ry v$VERSION"
ZIP_NAME="Ry2.zip"

echo "🚀 Starting Release Process for Ry $VERSION..."

# Git Add and Commit
git add .
git commit -m "$MSG"
git push origin main

# Create the Zip
echo "Zipping source code (excluding build artifacts)..."
cd ../
zip -r "$ZIP_NAME" . -x "build/*" "bin/*" ".git/*" ".vscode/*" "*.so" "*.o"
cd Ry2

# Tag Git
echo "Tagging version v$VERSION..."
git tag -a "v$VERSION" -m "$MSG"
git push origin "v$VERSION"

# Release to Github
echo "Creating GitHub Release and uploading $ZIP_NAME..."
gh release create "v$VERSION" "$ZIP_NAME" --title "$RELEASE_NAME" --notes "$MSG"

# Cleanup
echo "Cleaning up..."
rm "$ZIP_NAME"

echo "Successfully released Ry $VERSION!"