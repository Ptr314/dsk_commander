@ECHO OFF

call vars-mingw-latest.cmd

SET _ARCHITECTURE=x86_64
SET _PLATFORM=windows
SET _BUILD_DIR=.\build\%_PLATFORM%_%_ARCHITECTURE%
SET CC=%_ROOT_MINGW%\gcc.exe

FOR /F "tokens=* USEBACKQ" %%g IN (`findstr "PROJECT_VERSION" ..\src\globals.h`) do (SET VER=%%g)
for /f "tokens=3" %%G IN ("%VER%") DO (SET V=%%G)
set _VERSION=%V:"=%

SET _RELEASE_DIR=.\release\disk_commander-%_VERSION%-%_PLATFORM%-%_ARCHITECTURE%

if not exist %_BUILD_DIR%\ (
    call "%_ROOT_STATIC_BIN%\qt-cmake" -S ../src -B "%_BUILD_DIR%" -G Ninja

    cd "%_BUILD_DIR%"
    ninja

    cd ..\..\
)

mkdir "%_RELEASE_DIR%"

copy "%_BUILD_DIR%\DISKCommander.exe" "%_RELEASE_DIR%"


