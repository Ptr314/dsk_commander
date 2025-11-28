@ECHO OFF

SET _ROOT_MSVC=C:\DEV\MSVC\msvc
SET _ROOT_QT=C:\DEV\Qt\6.8.3\msvc2022_64
SET _ROOT_STATIC_BIN=%_ROOT_QT%\bin
SET _ROOT_MSVC_CMAKE=%_ROOT_QT%\bin\qt-cmake

REM Initialize Visual Studio build environment
call "%_ROOT_MSVC%\setup_x64.bat"
