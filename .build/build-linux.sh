#!/bin/bash
#
# Release build for Linux x86_64: AppImage via linuxdeployqt.
#
# Qt is linked dynamically here on purpose -- the AppImage bundles the Qt
# libraries it actually needs, and a shared build keeps compatibility with a
# wider range of distributions (see BUILD.md). The size of the binary itself
# is handled by the CMake options DC_OPTIMIZE_SIZE / DC_LTO, which are on by
# default in Release builds.
#
# Usage: ./build-linux.sh [clean]

set -euo pipefail

ARCHITECTURE="x86_64"
PLATFORM="linux"
QT_PATH="${HOME}/Qt/6.8.2/gcc_64"
LINUXDEPLOYQT="${HOME}/Downloads/linuxdeployqt-continuous-x86_64.AppImage"

cd "$(dirname "$0")"

BUILD_DIR="./build/${PLATFORM}-${ARCHITECTURE}"
VERSION=$(tr -d '\r\n' < ../VERSION)
RELEASE_DIR="./release/DISKCommander-${VERSION}-${PLATFORM}-${ARCHITECTURE}.AppDir"

if [ ! -x "${LINUXDEPLOYQT}" ]; then
    echo "ERROR: linuxdeployqt not found or not executable: ${LINUXDEPLOYQT}" >&2
    exit 1
fi

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}"
fi

cmake -DCMAKE_PREFIX_PATH="${QT_PATH}" -S ../src -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"

# The AppDir is recreated from scratch, otherwise leftovers from an earlier
# build end up inside the AppImage.
rm -rf "${RELEASE_DIR}"
mkdir -p "${RELEASE_DIR}/usr/bin"
cp -r ./.linux/DISKCommander.AppDir/* "${RELEASE_DIR}"
cp "${BUILD_DIR}/DISKCommander" "${RELEASE_DIR}/usr/bin/"

mkdir -p release
(
    cd release
    export VERSION="${VERSION}-${PLATFORM}"
    "${LINUXDEPLOYQT}" \
        "../${RELEASE_DIR}/usr/share/applications/DISKCommander.desktop" \
        -verbose=2 \
        -appimage \
        -no-translations \
        -no-copy-copyright-files \
        -qmake="${QT_PATH}/bin/qmake"
)

echo
echo "=== Release contents:"
ls -l release/*.AppImage
