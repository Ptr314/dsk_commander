@ECHO OFF

rem Environment for the x86_64 MSVC build.
rem
rem _ROOT_QT        -- regular (shared) Qt installation.
rem _ROOT_QT_STATIC -- static Qt build. When this directory exists,
rem                    build-win-msvc.bat picks it and produces a single exe
rem                    with no accompanying DLLs at all.
rem                    See BUILD.md for how to build it.

SET _ROOT_MSVC=C:\DEV\MSVC\msvc
SET _QT_VERSION=6.11.2
SET _ROOT_QT=C:\DEV\Qt\%_QT_VERSION%\msvc2022_64
SET _ROOT_QT_STATIC=C:\DEV\Qt\%_QT_VERSION%\msvc2022_64-static

REM Initialize Visual Studio build environment
call "%_ROOT_MSVC%\setup_x64.bat"
