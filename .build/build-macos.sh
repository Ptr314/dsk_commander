#!/bin/bash

QT_PATH="${HOME}/Qt-6.8.2-static-universal"

PLATFORM="macos"
APP_NAME="DISKCommander"
BUILD_DIR="./build/${PLATFORM}"
RELEASE_DIR="./release"

VERSION=$(cat ../VERSION | tr -d '\r\n')

cmake -DCMAKE_PREFIX_PATH=${QT_PATH} -S ../src -B ${BUILD_DIR} -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cwd=$(pwd)
cd "$BUILD_DIR"
ninja

${QT_PATH}/bin/macdeployqt ${APP_NAME}.app -dmg

cd $cwd
mkdir $RELEASE_DIR
cp ${BUILD_DIR}/${APP_NAME}.dmg ${RELEASE_DIR}/disk_commander-${VERSION}-${PLATFORM}.dmg
