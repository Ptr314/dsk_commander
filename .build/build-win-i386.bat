@ECHO OFF

CALL vars-mingw-qt5.15.cmd

SET _ARCHITECTURE=i386
SET _PLATFORM=windows
SET _BUILD_DIR=.\build\%_PLATFORM%_%_ARCHITECTURE%

FOR /F "tokens=* USEBACKQ" %%g IN (`findstr "PROJECT_VERSION" ..\src\globals.h`) do (SET VER=%%g)
for /f "tokens=3" %%G IN ("%VER%") DO (SET V=%%G)
set _VERSION=%V:"=%

SET _RELEASE_DIR=".\release\disk_commander-%_VERSION%-%_PLATFORM%-%_ARCHITECTURE%"

if not exist %_BUILD_DIR%\ (
    set CC=%_ROOT_MINGW%\gcc.exe
    cmake -DCMAKE_PREFIX_PATH="%_ROOT_STATIC%" -S ../src -B "%_BUILD_DIR%" -G Ninja

    cd "%_BUILD_DIR%"
    ninja

    cd ..\..\
)

mkdir "%_RELEASE_DIR%"

xcopy ..\deploy\* "%_RELEASE_DIR%" /E
rem del %_RELEASE_DIR%\ecat.ini
rem ren %_RELEASE_DIR%\.ecat.ini ecat.ini

copy "%_BUILD_DIR%\DISKCommander.exe" "%_RELEASE_DIR%"

copy "%_ROOT_MINGW%\libgcc_s_dw2-1.dll" "%_RELEASE_DIR%"
copy "%_ROOT_MINGW%\libstdc++-6.dll" "%_RELEASE_DIR%"
copy "%_ROOT_MINGW%\libwinpthread-1.dll" "%_RELEASE_DIR%"




