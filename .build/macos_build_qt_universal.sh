#!/bin/bash

set -e

QT_VERSION=6.8.2
QT_SRC_TAR=~/Downloads/qt-everywhere-src-$QT_VERSION.tar.xz
QT_SRC_DIR=/tmp/qt-everywhere-src-$QT_VERSION
INSTALL_BASE=~/Qt-$QT_VERSION-static
BUILD_BASE=~/dev/qt-build

# Распаковываем исходники
echo "Extracting Qt sources..."
rm -rf "$QT_SRC_DIR"
mkdir -p "$QT_SRC_DIR"
tar xf "$QT_SRC_TAR" -C /tmp

# Сборка x86_64
echo "Building x86_64..."
mkdir -p "$BUILD_BASE-x86_64"
cd "$BUILD_BASE-x86_64"
$QT_SRC_DIR/configure \
    -static -release -nomake tests -nomake examples \
    -opensource -confirm-license \
    -platform macx-clang \
    -no-rpath \
    -prefix $INSTALL_BASE-x86_64 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build . --parallel
cmake --install .

# Сборка arm64
echo "Building arm64..."
mkdir -p "$BUILD_BASE-arm64"
cd "$BUILD_BASE-arm64"
$QT_SRC_DIR/configure \
    -static -release -nomake tests -nomake examples \
    -opensource -confirm-license \
    -platform macx-clang \
    -no-rpath \
    -prefix $INSTALL_BASE-arm64 \
    -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build . --parallel
cmake --install .

# Объединение в universal
UNIVERSAL_DIR=$INSTALL_BASE-universal
echo "Creating universal binaries at $UNIVERSAL_DIR..."
mkdir -p "$UNIVERSAL_DIR/lib"

cd "$INSTALL_BASE-x86_64/lib"
for lib in *.a; do
    echo "Merging $lib..."
    lipo -create \
        "$INSTALL_BASE-x86_64/lib/$lib" \
        "$INSTALL_BASE-arm64/lib/$lib" \
        -output "$UNIVERSAL_DIR/lib/$lib"
done

# Копируем include и bin (только из x86_64, как базу)
echo "Copying includes and binaries..."
cp -R "$INSTALL_BASE-x86_64/include" "$UNIVERSAL_DIR/"
cp -R "$INSTALL_BASE-x86_64/bin" "$UNIVERSAL_DIR/"

echo "Done! Universal Qt static libs are in: $UNIVERSAL_DIR"
