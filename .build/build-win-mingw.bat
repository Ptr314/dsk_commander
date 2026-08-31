@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

REM ---------------------------------------------------------------------------
REM Release build, x86_64, mingw.
REM
REM If vars-mingw-latest.cmd points at an existing static Qt prefix
REM (_QT_PREFIX_STATIC), that kit is used and the result is a single exe with
REM no DLLs beside it. Otherwise a shared build is produced and Qt6*.dll, the
REM platform/style plugins and the mingw runtime are copied next to it.
REM
REM NOTE: in a shared build the mingw runtime (libstdc++-6.dll and friends)
REM cannot be dropped -- the Qt6*.dll themselves import it, not just our exe.
REM A static Qt is the only way to get a DLL-free build here.
REM
REM Pass "clean" to wipe the build directory first.
REM ---------------------------------------------------------------------------

cd /d "%~dp0"
call "%~dp0vars-mingw-latest.cmd" || exit /b 1

SET _ARCHITECTURE=x86_64
SET _COMPILER=mingw
SET _PLATFORM=windows
SET _BUILD_DIR=.\build\%_PLATFORM%_%_ARCHITECTURE%_%_COMPILER%
SET CC=%_ROOT_MINGW%\gcc.exe

set /p _VERSION=<..\VERSION

if exist "%_QT_PREFIX_STATIC%\bin\qt-cmake.bat" (
    SET "_QT_KIT=%_QT_PREFIX_STATIC%"
    SET _QT_STATIC=1
) else (
    SET "_QT_KIT=%_QT_PREFIX%"
    SET _QT_STATIC=0
    echo WARNING: no static Qt in "%_QT_PREFIX_STATIC%", falling back to a shared build.
)

SET _RELEASE_NAME=disk_commander-%_VERSION%-%_PLATFORM%-%_ARCHITECTURE%-%_COMPILER%
SET _RELEASE_DIR=.\release\%_RELEASE_NAME%

if /I "%~1"=="clean" if exist "%_BUILD_DIR%" rmdir /s /q "%_BUILD_DIR%"
call "%~dp0win-common.cmd" checkgen "%_BUILD_DIR%" Ninja || exit /b 1

REM Always reconfigure and rebuild: cmake and ninja work out what actually
REM changed. Previously the whole build was skipped when the directory already
REM existed, so a stale executable could be packaged into the release.
call "%_QT_KIT%\bin\qt-cmake" -S ../src -B "%_BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build "%_BUILD_DIR%" || exit /b 1

call "%~dp0win-common.cmd" reset "%_RELEASE_DIR%" || exit /b 1
copy /y "%_BUILD_DIR%\DISKCommander.exe" "%_RELEASE_DIR%" >nul || exit /b 1

if "%_QT_STATIC%"=="1" (
    echo Static Qt build: no runtime DLLs needed.
) else (
    echo Copying Qt runtime from "%_QT_KIT%"
    copy /y "%_QT_KIT%\bin\Qt6Core.dll"    "%_RELEASE_DIR%" >nul || exit /b 1
    copy /y "%_QT_KIT%\bin\Qt6Gui.dll"     "%_RELEASE_DIR%" >nul || exit /b 1
    copy /y "%_QT_KIT%\bin\Qt6Widgets.dll" "%_RELEASE_DIR%" >nul || exit /b 1

    mkdir "%_RELEASE_DIR%\platforms"
    copy /y "%_QT_KIT%\plugins\platforms\qwindows.dll" "%_RELEASE_DIR%\platforms\" >nul || exit /b 1

    mkdir "%_RELEASE_DIR%\styles"
    copy /y "%_QT_KIT%\plugins\styles\qmodernwindowsstyle.dll" "%_RELEASE_DIR%\styles\" >nul || exit /b 1

    echo Copying mingw runtime from "%_ROOT_MINGW%"
    copy /y "%_ROOT_MINGW%\libgcc_s_seh-1.dll"  "%_RELEASE_DIR%" >nul || exit /b 1
    copy /y "%_ROOT_MINGW%\libstdc++-6.dll"     "%_RELEASE_DIR%" >nul || exit /b 1
    copy /y "%_ROOT_MINGW%\libwinpthread-1.dll" "%_RELEASE_DIR%" >nul || exit /b 1
)

call "%~dp0win-common.cmd" report "%_RELEASE_DIR%"
call "%~dp0win-common.cmd" zip "%_RELEASE_DIR%" "%_RELEASE_NAME%" || exit /b 1

ENDLOCAL
