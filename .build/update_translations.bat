@echo off
rem SET BUILD_DIR=build/Desktop_Qt_6_8_3_MinGW_64_bit-Debug
SET BUILD_DIR=cmake-build-debug-qt-683-msvc
if exist "../src/%BUILD_DIR%" (
  cd ../src
  cmake.exe --build "./%BUILD_DIR%" --target update_translations
) else (
  echo "ERROR: Build directory doesn't exist: ../src/%BUILD_DIR%"
)