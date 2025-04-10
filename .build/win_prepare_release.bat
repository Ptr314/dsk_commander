@echo off
setlocal enabledelayedexpansion

:: === ���� ===
set ROOT_DIR=..
set SRC_DIR=%ROOT_DIR%\src
set LIB_DIR=%SRC_DIR%\libs\dsk_tools
set CMAKE_FILE=%SRC_DIR%\CMakeLists.txt

:: === ������ ������ �� CMakeLists.txt ===
for /f "tokens=3 delims= " %%v in ('findstr /R /C:"project(.* VERSION .* LANGUAGES" %CMAKE_FILE%') do (
    set "TAG_NAME=%%v"
)

if "%TAG_NAME%"=="" (
    echo ������: �� ������� ������� ������ �� CMakeLists.txt
    exit /b 1
)

echo ���������� ������: %TAG_NAME%

:: === �������� ������� ����� �������� ���� ===
for /f "delims=" %%b in ('git -C %ROOT_DIR% rev-parse --abbrev-ref HEAD') do set CURRENT_BRANCH=%%b

if /I NOT "!CURRENT_BRANCH!"=="dev" (
    echo ������: ������� ����� "!CURRENT_BRANCH!" (����� dev)
    exit /b 1
)

:: === �������� �� ��������������� ��������� ===
git -C %ROOT_DIR% diff-index --quiet HEAD --
if errorlevel 1 (
    echo ������: ���� ��������������� ��������� � �������� �������. ������� ������� ��� ������ stash.
    exit /b 1
)

:: === �������� ������������� ���� ===
git -C %ROOT_DIR% fetch --tags >nul
git -C %ROOT_DIR% tag | findstr /x "%TAG_NAME%" >nul
if not errorlevel 1 (
    echo ������: ��� "%TAG_NAME%" ��� ����������.
    exit /b 1
)

:: === ������ � ���������� ===
echo === ������ � ����������� ===
cd %LIB_DIR%

for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set LIB_BRANCH=%%b
if /I NOT "!LIB_BRANCH!"=="dev" (
    echo ������������ �� ����� dev � ����������...
    git checkout dev
)

echo ��������� ���������� � ���� ����������...
git fetch origin
git checkout master
git merge origin/dev -m "Merge dev into master for release"
git push origin master

cd %ROOT_DIR%

:: === ���������� ��������� � �������� ������� ===
cd %LIB_DIR%
git checkout master
cd %ROOT_DIR%

git add %LIB_DIR%
git commit -m "Update dsk_tools submodule to master for release %TAG_NAME%"

:: === ������ dev > master � �������� ������� ===
echo ���� ����� dev � master...
git -C %ROOT_DIR% checkout master
git -C %ROOT_DIR% merge dev -m "Merge dev into master for release %TAG_NAME%"
git -C %ROOT_DIR% push origin master

:: === ��������� ������ submodule (�� master) ===
git add %LIB_DIR%
git commit -m "Finalize submodule state for %TAG_NAME%"
git push origin master

:: === �������� � ��� ���� ===
echo �������� ���� %TAG_NAME%...
git tag -a %TAG_NAME% -m "Release %TAG_NAME%"
git push origin %TAG_NAME%

echo ����� %TAG_NAME% �������� �������.

exit /b 0
