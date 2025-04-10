@echo off
setlocal enabledelayedexpansion

:: === Пути ===
set ROOT_DIR=..
set SRC_DIR=%ROOT_DIR%\src
set LIB_DIR=%SRC_DIR%\libs\dsk_tools
set CMAKE_FILE=%SRC_DIR%\CMakeLists.txt

:: === Чтение версии из CMakeLists.txt ===
for /f "tokens=3 delims= " %%v in ('findstr /R /C:"project(.* VERSION .* LANGUAGES" %CMAKE_FILE%') do (
    set "TAG_NAME=%%v"
)

if "%TAG_NAME%"=="" (
    echo Ошибка: не удалось извлечь версию из CMakeLists.txt
    exit /b 1
)

echo Обнаружена версия: %TAG_NAME%

:: === Проверка текущей ветки основной репы ===
for /f "delims=" %%b in ('git -C %ROOT_DIR% rev-parse --abbrev-ref HEAD') do set CURRENT_BRANCH=%%b

if /I NOT "!CURRENT_BRANCH!"=="dev" (
    echo Ошибка: текущая ветка "!CURRENT_BRANCH!" (нужна dev)
    exit /b 1
)

:: === Проверка на незакоммиченные изменения ===
git -C %ROOT_DIR% diff-index --quiet HEAD --
if errorlevel 1 (
    echo Ошибка: есть незакоммиченные изменения в основном проекте. Заверши коммиты или сделай stash.
    exit /b 1
)

:: === Проверка существования тега ===
git -C %ROOT_DIR% fetch --tags >nul
git -C %ROOT_DIR% tag | findstr /x "%TAG_NAME%" >nul
if not errorlevel 1 (
    echo Ошибка: тег "%TAG_NAME%" уже существует.
    exit /b 1
)

:: === Работа с подмодулем ===
echo === Работа с библиотекой ===
cd %LIB_DIR%

for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set LIB_BRANCH=%%b
if /I NOT "!LIB_BRANCH!"=="dev" (
    echo Переключение на ветку dev в библиотеке...
    git checkout dev
)

echo Получение обновлений и мерж библиотеки...
git fetch origin
git checkout master
git merge origin/dev -m "Merge dev into master for release"
git push origin master

cd %ROOT_DIR%

:: === Обновление подмодуля в основном проекте ===
cd %LIB_DIR%
git checkout master
cd %ROOT_DIR%

git add %LIB_DIR%
git commit -m "Update dsk_tools submodule to master for release %TAG_NAME%"

:: === Мержим dev > master в основном проекте ===
echo Мерж ветки dev в master...
git -C %ROOT_DIR% checkout master
git -C %ROOT_DIR% merge dev -m "Merge dev into master for release %TAG_NAME%"
git -C %ROOT_DIR% push origin master

:: === Финальный коммит submodule (на master) ===
git add %LIB_DIR%
git commit -m "Finalize submodule state for %TAG_NAME%"
git push origin master

:: === Создание и пуш тега ===
echo Создание тега %TAG_NAME%...
git tag -a %TAG_NAME% -m "Release %TAG_NAME%"
git push origin %TAG_NAME%

echo Релиз %TAG_NAME% завершён успешно.

exit /b 0
