@ECHO OFF

call vars-mingw-latest.cmd

SET _ARCHITECTURE=x86_64
SET _COMPILER=mingw
SET _PLATFORM=windows
SET _BUILD_DIR=.\build\%_PLATFORM%_%_ARCHITECTURE%_%_COMPILER%
SET CC=%_ROOT_MINGW%\gcc.exe

set /p _VERSION=<..\VERSION

SET _RELEASE_NAME="disk_commander-%_VERSION%-%_PLATFORM%-%_ARCHITECTURE%-%_COMPILER%"
SET _RELEASE_DIR=".\release\%_RELEASE_NAME%"

if not exist %_BUILD_DIR%\ (
    call "%_ROOT_BIN%\qt-cmake" -S ../src -B "%_BUILD_DIR%" -G Ninja

    cd "%_BUILD_DIR%"
    ninja

    cd ..\..\
)

mkdir "%_RELEASE_DIR%"

copy "%_BUILD_DIR%\DISKCommander.exe" "%_RELEASE_DIR%"

echo "Copying Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll from %_ROOT_BIN%"
copy "%_ROOT_BIN%\Qt6Core.dll" "%_RELEASE_DIR%"
copy "%_ROOT_BIN%\Qt6Gui.dll" "%_RELEASE_DIR%"
copy "%_ROOT_BIN%\Qt6Widgets.dll" "%_RELEASE_DIR%"

mkdir "%_RELEASE_DIR%\platforms"
echo "Copying platforms\qwindows.dll from %_QT_PLUGINS%"
copy "%_QT_PLUGINS%\platforms\qwindows.dll" "%_RELEASE_DIR%\platforms"

mkdir "%_RELEASE_DIR%\styles"
echo "Copying styles\qmodernwindowsstyle.dll from %_QT_PLUGINS%"
copy "%_QT_PLUGINS%\styles\qmodernwindowsstyle.dll" "%_RELEASE_DIR%\styles"

echo "Copying libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll from %_ROOT_MINGW%"
copy "%_ROOT_MINGW%\libgcc_s_seh-1.dll" "%_RELEASE_DIR%"
copy "%_ROOT_MINGW%\libstdc++-6.dll" "%_RELEASE_DIR%"
copy "%_ROOT_MINGW%\libwinpthread-1.dll" "%_RELEASE_DIR%"

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

