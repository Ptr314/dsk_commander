#!/bin/bash

ARCHITECTURE="x86_64"
PLATFORM="linux"
QT_PATH=~/Qt/6.8.2/gcc_64
LINUXDEPLOYQT="~/Downloads/linuxdeployqt-continuous-x86_64.AppImage"

BUILD_DIR="./build/${PLATFORM}-${ARCHITECTURE}"

VERSION=$(cat ../VERSION | tr -d '\r\n')

RELEASE_DIR="./release/DISKCommander-${VERSION}-${PLATFORM}-${ARCHITECTURE}.AppDir"

cmake -DCMAKE_PREFIX_PATH=${QT_PATH} -S ../src -B ${BUILD_DIR} -G Ninja -DCMAKE_BUILD_TYPE=Release

CWD=$(pwd)
cd ${BUILD_DIR}
ninja
cd ${CWD}

mkdir -p ${RELEASE_DIR}/usr/bin
cp -r ./.linux/DISKCommander.AppDir/* ${RELEASE_DIR}
cp "${BUILD_DIR}/DISKCommander" "${RELEASE_DIR}/usr/bin/"

cd release

export VERSION=${VERSION}-${PLATFORM}
exec ${LINUXDEPLOYQT} ../${RELEASE_DIR}/usr/share/applications/DISKCommander.desktop -verbose=2 -appimage -no-translations -qmake=${QT_PATH}/bin/qmake

cd $CWD
