#!/bin/bash

ARCHITECTURE="x86_64"
PLATFORM="macos"
QT_PATH="/usr/local/Qt-6.8.2-static"
BUILD_DIR="./build/${PLATFORM}-${ARCHITECTURE}"
RELEASE_DIR="./release"
RESOURCES=./DISK_Commander.app/Contents/Resources

VERSION=`cat ../src/globals.h | grep 'PROJECT_VERSION' | awk '{printf $3}' | tr -d '"\n\r'`

cmake -DCMAKE_PREFIX_PATH=${QT_PATH} -S ../src -B ${BUILD_DIR} -G Ninja

cwd=$(pwd)
cd "$BUILD_DIR"
ninja

macdeployqt DISK_Commander.app -dmg
cd $cwd
mkdir $RELEASE_DIR
cp ${BUILD_DIR}/DISK_Commander.dmg ${RELEASE_DIR}/disk_commander-${VERSION}-${PLATFORM}-${ARCHITECTURE}.dmg
