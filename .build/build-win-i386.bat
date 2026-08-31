@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

REM ---------------------------------------------------------------------------
REM Release build, i386, for Windows XP: Qt 5.6.3 + mingw 4.9.2.
REM
REM Qt 5.6.3 cannot be configured as a static build here (see BUILD.md), so a
REM minimal set of DLLs is shipped next to the exe. The mingw runtime cannot be
REM dropped -- the Qt5*.dll themselves import it.
REM
REM Pass "clean" to wipe the build directory first.
REM ---------------------------------------------------------------------------

cd /d "%~dp0"
call "%~dp0vars-mingw-qt5.6.cmd" || exit /b 1

SET _ARCHITECTURE=i386
SET _PLATFORM=windows
SET _BUILD_DIR=.\build\%_PLATFORM%_%_ARCHITECTURE%
SET CC=%_ROOT_MINGW%\gcc.exe

set /p _VERSION=<..\VERSION

SET _RELEASE_NAME=disk_commander-%_VERSION%-%_PLATFORM%-%_ARCHITECTURE%
SET _RELEASE_DIR=.\release\%_RELEASE_NAME%

if /I "%~1"=="clean" if exist "%_BUILD_DIR%" rmdir /s /q "%_BUILD_DIR%"
call "%~dp0win-common.cmd" checkgen "%_BUILD_DIR%" Ninja || exit /b 1

REM Always reconfigure and rebuild: cmake and ninja work out what actually
REM changed. Previously the whole build was skipped when the directory already
REM existed, so a stale executable could be packaged into the release.
cmake -DCMAKE_PREFIX_PATH="%_QT_PREFIX%" -S ../src -B "%_BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build "%_BUILD_DIR%" || exit /b 1

call "%~dp0win-common.cmd" reset "%_RELEASE_DIR%" || exit /b 1
copy /y "%_BUILD_DIR%\DISKCommander.exe" "%_RELEASE_DIR%" >nul || exit /b 1

echo Copying Qt runtime from "%_QT_PREFIX%"
copy /y "%_QT_PREFIX%\bin\Qt5Core.dll"    "%_RELEASE_DIR%" >nul || exit /b 1
copy /y "%_QT_PREFIX%\bin\Qt5Gui.dll"     "%_RELEASE_DIR%" >nul || exit /b 1
copy /y "%_QT_PREFIX%\bin\Qt5Widgets.dll" "%_RELEASE_DIR%" >nul || exit /b 1

mkdir "%_RELEASE_DIR%\platforms"
copy /y "%_QT_PLUGINS%\platforms\qwindows.dll" "%_RELEASE_DIR%\platforms\" >nul || exit /b 1

echo Copying mingw runtime from "%_ROOT_MINGW%"
copy /y "%_ROOT_MINGW%\libgcc_s_dw2-1.dll"  "%_RELEASE_DIR%" >nul || exit /b 1
copy /y "%_ROOT_MINGW%\libstdc++-6.dll"     "%_RELEASE_DIR%" >nul || exit /b 1
copy /y "%_ROOT_MINGW%\libwinpthread-1.dll" "%_RELEASE_DIR%" >nul || exit /b 1

call "%~dp0win-common.cmd" report "%_RELEASE_DIR%"
call "%~dp0win-common.cmd" zip "%_RELEASE_DIR%" "%_RELEASE_NAME%" || exit /b 1

ENDLOCAL
