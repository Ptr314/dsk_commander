@ECHO OFF

CALL vars-mingw-qt5.6.cmd

SET _ARCHITECTURE=i386
SET _PLATFORM=windows
SET _BUILD_DIR=.\build\%_PLATFORM%_%_ARCHITECTURE%
SET _QT_PATH="%_ROOT_QT%\%_QT_VERSION%"

set /p _VERSION=<..\VERSION

SET _RELEASE_NAME="disk_commander-%_VERSION%-%_PLATFORM%-%_ARCHITECTURE%"
SET _RELEASE_DIR=".\release\%_RELEASE_NAME%"

if not exist %_BUILD_DIR%\ (
    set CC=%_ROOT_MINGW%\gcc.exe
    cmake -DCMAKE_PREFIX_PATH="%_QT_PATH%" -S ../src -B "%_BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release

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

set SEVENZIP="7z"
%SEVENZIP% >nul 2>&1
if errorlevel 9009 (
    if exist "C:\Program Files\7-Zip\7z.exe" (
        set SEVENZIP="C:\Program Files\7-Zip\7z.exe"
    ) else if exist "C:\Program Files (x86)\7-Zip\7z.exe" (
        set SEVENZIP="C:\Program Files (x86)\7-Zip\7z.exe"
    ) else (
        echo ERROR: 7z.exe not found. Please install 7-Zip or add it to PATH.
        exit /b 1
    )
)

pushd "%_RELEASE_DIR%"
%SEVENZIP% a "..\%_RELEASE_NAME%.zip" * -mx9
popd



