@echo off
rem Run lupdate/lupdate-pro at the caller's privilege level so Windows UAC
rem installer-detection (triggered by "update" in the filename) does not
rem pop "allow ... to make changes to your device" prompts.
SET __COMPAT_LAYER=RunAsInvoker
rem The line above fixes the prompt only for this batch. To also silence it
rem when running lupdate from Qt Creator, set a permanent per-user (HKCU)
rem compatibility layer once in PowerShell (adjust the path on Qt upgrade):
rem   $layers = 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers'
rem   New-Item -Path $layers -Force | Out-Null
rem   Set-ItemProperty -Path $layers -Name "C:\DEV\Qt\6.10.2\mingw_64\bin\lupdate.exe"     -Value "~ RUNASINVOKER"
rem   Set-ItemProperty -Path $layers -Name "C:\DEV\Qt\6.10.2\mingw_64\bin\lupdate-pro.exe" -Value "~ RUNASINVOKER"
SET BUILD_DIR=cmake-build-qt-6.11.1-mingw_1310
if exist "../src/%BUILD_DIR%" (
  cd ../src
  cmake.exe --build "./%BUILD_DIR%" --target update_translations
) else (
  echo "ERROR: Build directory doesn't exist: ../src/%BUILD_DIR%"
)