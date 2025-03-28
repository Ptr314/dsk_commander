@ECHO OFF

CALL vars-mingw-qt5.6.cmd

SET _ARCHITECTURE=i386
SET _PLATFORM=windows
SET _BUILD_DIR=.\build\%_PLATFORM%_%_ARCHITECTURE%
SET _QT_PATH="%_ROOT_QT%\%_QT_VERSION%"

FOR /F "tokens=* USEBACKQ" %%g IN (`findstr "PROJECT_VERSION" ..\src\globals.h`) do (SET VER=%%g)
for /f "tokens=3" %%G IN ("%VER%") DO (SET V=%%G)
set _VERSION=%V:"=%

SET _RELEASE_DIR=".\release\disk_commander-%_VERSION%-%_PLATFORM%-%_ARCHITECTURE%"

if not exist %_BUILD_DIR%\ (
    set CC=%_ROOT_MINGW%\gcc.exe
    cmake -DCMAKE_PREFIX_PATH="%_QT_PATH%" -S ../src -B "%_BUILD_DIR%" -G Ninja

    cd "%_BUILD_DIR%"
    ninja

    cd ..\..\
)

mkdir "%_RELEASE_DIR%"

copy "%_BUILD_DIR%\DISKCommander.exe" "%_RELEASE_DIR%"

copy "%_QT_PATH%\bin\Qt5Core.dll" "%_RELEASE_DIR%"
copy "%_QT_PATH%\bin\Qt5Gui.dll" "%_RELEASE_DIR%"
copy "%_QT_PATH%\bin\Qt5Widgets.dll" "%_RELEASE_DIR%"

copy "%_ROOT_MINGW%\libgcc_s_dw2-1.dll" "%_RELEASE_DIR%"
copy "%_ROOT_MINGW%\libstdc++-6.dll" "%_RELEASE_DIR%"
copy "%_ROOT_MINGW%\libwinpthread-1.dll" "%_RELEASE_DIR%"

mkdir "%_RELEASE_DIR%\platforms"
copy "%_QT_PATH%\plugins\platforms\qwindows.dll" "%_RELEASE_DIR%\platforms"



