#!/bin/bash
#
# Release build for macOS: universal (x86_64 + arm64) .dmg.
#
# Qt is linked statically (see BUILD.md and macos_build_qt_universal.sh), so
# the bundle carries no Qt frameworks and macdeployqt only has to produce the
# disk image. The size of the binary itself is handled by the CMake options
# DC_OPTIMIZE_SIZE / DC_LTO, which are on by default in Release builds.
#
# Usage: ./build-macos.sh [clean]

set -euo pipefail

QT_PATH="${HOME}/Qt-6.8.2-static-universal"

PLATFORM="macos"
APP_NAME="DISKCommander"

cd "$(dirname "$0")"

BUILD_DIR="./build/${PLATFORM}"
RELEASE_DIR="./release"
VERSION=$(tr -d '\r\n' < ../VERSION)
DMG_NAME="disk_commander-${VERSION}-${PLATFORM}.dmg"

if [ ! -x "${QT_PATH}/bin/macdeployqt" ]; then
    echo "ERROR: Qt not found at ${QT_PATH}" >&2
    exit 1
fi

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}"
fi

cmake -DCMAKE_PREFIX_PATH="${QT_PATH}" -S ../src -B "${BUILD_DIR}" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build "${BUILD_DIR}"

# macdeployqt refuses to overwrite an existing image, and a stale .dmg here
# would silently be shipped as the new release.
rm -f "${BUILD_DIR}/${APP_NAME}.dmg"
"${QT_PATH}/bin/macdeployqt" "${BUILD_DIR}/${APP_NAME}.app" -dmg

mkdir -p "${RELEASE_DIR}"
cp "${BUILD_DIR}/${APP_NAME}.dmg" "${RELEASE_DIR}/${DMG_NAME}"

echo
echo "=== Release contents:"
ls -l "${RELEASE_DIR}/${DMG_NAME}"
ls -l "${BUILD_DIR}/${APP_NAME}.app/Contents/MacOS/${APP_NAME}"
