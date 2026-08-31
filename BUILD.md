# Настройка окружения и компиляция приложения

Программа написана как проект cmake с использованием библиотеки Qt. Ниже описывается установка окружения под разные ОС и компиляция приложения.

В данный момент программа компилируется под следующие платформы:

* Windows XP+
    * Версия i386 на основе Qt 5.6.3 и mingw 4.9.2.
* Windows 10+
    * Версия х86_64, MSVC 2022 (основная сборка релиза). Актуальная версия Qt 6.11.2.
    * Версия х86_64, mingw 13.1 (альтернативная сборка).
* macOS 15 (возможна совместимость с более ранними версиями)
    * Универсальная версия х86_64+arm64. Qt 6.8.2, xcode 16
* Linux Ubuntu 20.04+
    * Версия х86_64. Qt 6.8.2, gcc 9.4.0

Версия х86_64+arm64 для macOS использует статическую сборку Qt. Для Windows статическая сборка используется, если соответствующий каталог Qt существует (см. ниже) -- тогда на выходе получается один exe без единой сопутствующей DLL. Версия для Linux использует динамическую сборку в целях лучшей совместимости с разными дистрибутивами, всё необходимое упаковывается в AppImage. Компиляция происходит в Ubuntu 20.04.

---
## Размер исполняемого файла

Оптимизация размера включена в `src/CMakeLists.txt` и действует на все платформы в конфигурациях `Release` и `MinSizeRel`. Отдельных действий при сборке не требуется, но поведение можно поменять двумя опциями cmake:

| Опция | По умолчанию | Что делает |
|---|---|---|
| `DC_OPTIMIZE_SIZE` | `ON` | `-O2` вместо `-O3`; `-ffunction-sections -fdata-sections` + `--gc-sections` (на macOS `-dead_strip`, на MSVC `/Gy /Gw` + `/OPT:REF /OPT:ICF`); удаление таблицы символов из релизного бинарника |
| `DC_LTO` | `ON` | Межмодульная оптимизация (LTO/IPO). Автоматически отключается там, где тулчейн её не тянет -- в частности для gcc < 8, то есть для сборки i386 под Windows XP |

Отключить, например, так: `cmake ... -DDC_LTO=OFF`.

Порядок величин (Windows, версия 2.8.1):

| Сборка | Было | Стало |
|---|---|---|
| x86_64 mingw, exe | 7.0 МБ | 2.0 МБ |
| i386 Qt 5.6, exe | 6.9 МБ | 2.1 МБ |
| x86_64 MSVC, zip релиза | 13.3 МБ (6 файлов) | 9.8 МБ (один exe) |

Кроме того, при статической сборке Qt из exe исключаются ненужные плагины (форматы изображений, iconengines, SQL, сеть, TLS, печать) -- приложение работает только с PNG, поддержка которого встроена в QtGui.

---
## Windows

#### 1. Установить программы
* https://download.qt.io/, скачать online-инсталлятор (возможно, из России понадобится зарубежный VPN) и установить следующие компоненты:
    * Qt [X.X.X]
        * MinGW YY.Y.Y
        * Sources (Если не нужна компиляция статической версии для релиза, можно пропустить)
        * Plugins
            * Qt5Compatibility 
        * Qt Creator
        * Mingw YY.Y.Y (Версия, соответствующая компилятору библиотеки в предыдущем пункте)
        * Mingw 8.1.0 (Версия для сборки i386 для Windows 7)
        * Mingw 4.9.2 (Версия для сборки i386 для Windows XP)
        * cmake
        * ninja

#### 2. Настроить debug-версию в Qt Creator
* Если нужна полная очистка, удалить файлы __CMakeLists.txt.user*__.

#### 3. Компиляция статической версии Qt

Статическая сборка Qt -- единственный способ получить приложение без сопутствующих DLL. Причём убрать только рантайм компилятора (`libstdc++-6.dll` и т.п.) при динамической Qt нельзя: от него зависят не только наш exe, но и сами `Qt6*.dll`.

Скрипты сборки сами определяют, есть ли статическая сборка Qt, и если есть -- используют её и не копируют рядом с exe ни одной DLL. Пути задаются в `vars-*.cmd`:

* `vars-msvc-latest.cmd` -> `_ROOT_QT_STATIC` (по умолчанию `C:\DEV\Qt\X.X.X\msvc2022_64-static`)
* `vars-mingw-latest.cmd` -> `_QT_PREFIX_STATIC` (по умолчанию `C:\DEV\Qt\X.X.X-static`)

При переходе на новую версию Qt не забудьте поправить `_QT_VERSION` в этих же файлах, иначе скрипт не найдёт статическую сборку и молча соберёт динамическую.

##### Qt6, MSVC (основная сборка релиза)

```
cd репозиторий-приложения\.build
%SystemRoot%\system32\cmd.exe /E:ON /V:ON /k vars-msvc-latest.cmd
cd C:\Temp
mkdir qt-build-msvc
cd qt-build-msvc
C:\DEV\Qt\%_QT_VERSION%\Src\configure.bat -static -static-runtime -release -opensource -confirm-license -nomake examples -nomake tests -submodules qtbase,qttools,qttranslations -prefix C:\DEV\Qt\%_QT_VERSION%\msvc2022_64-static
cmake --build . --parallel
cmake --install .
```

`-static-runtime` линкует и рантайм MSVC, поэтому на целевой машине не нужен и распространяемый пакет Visual C++.

`-submodules` ограничивает сборку тремя нужными модулями и их зависимостями вместо всего qt-everywhere. Помимо экономии времени это обходит и ошибку сборки: плагин SAPI из qtspeech требует ATL (`atlbase.h`), которого нет в портативной установке MSVC из `portable-msvc.py`:

```
sphelper.h(51): fatal error C1083: Cannot open include file: 'atlbase.h'
```

Зачем нужен каждый модуль:

* `qtbase` -- Core, Gui, Widgets;
* `qttools` -- `lrelease` и `lupdate`, без них падает `find_package(Qt6 ... LinguistTools)`;
* `qttranslations` -- `qtbase_ru.qm` и `qtbase_en.qm`, без них `src/CMakeLists.txt` останавливается с `FATAL_ERROR`.

Для смены набора модулей configure нужно запускать в пустом каталоге сборки.

##### Qt6, mingw

https://doc.qt.io/qt-6/windows-building.html

* Отредактировать `.build/vars-mingw-latest.cmd` на действительные пути.
* Открыть командную строку и скомпилировать Qt:

```
cd репозиторий-приложения\.build
%SystemRoot%\system32\cmd.exe /E:ON /V:ON /k vars-mingw-latest.cmd
cd C:\Temp
mkdir qt-build
cd qt-build
configure.bat -static -static-runtime -release -opensource -confirm-license -nomake examples -nomake tests -submodules qtbase,qttools,qttranslations -prefix c:\DEV\Qt\%_QT_VERSION%-static
cmake --build . --parallel
cmake --install .
```

##### Qt5 для Windows XP

Для XP необходима версия Qt 5.6.3 (https://download.qt.io/new_archive/qt/5.6/5.6.3/single/) и mingw 4.9.2 (https://wiki.qt.io/MinGW)

~~~
cd репозиторий-приложения\.build
%SystemRoot%\system32\cmd.exe /E:ON /V:ON /k vars-mingw-qt5.6.cmd
cd C:\Temp
mkdir qt5.6-build
cd qt5.6-build
configure.bat -release -nomake examples -nomake tests -opensource -confirm-license -no-opengl -target xp -no-directwrite -no-compile-examples -skip qtdeclarative -prefix c:\DEV\Qt\%_QT_VERSION%
mingw32-make
mingw32-make install
~~~

Примечания: 
* `-target xp` необходимо для компиляции в формат .exe Windows XP.
* `-skip qtdeclarative` позволяет избежать необходимости в установке Python, но отключает модули QtQuick, QtQml и некоторые другие.
* Собрать статическую версию не удается, поэтому в дальнейшем необходимо в папку программы помещать следующие файлы:
    * `Qt5Core.dll`, `Qt5Gui.dll`, `Qt5Widgets.dll` из `Qt/5.6.3/bin/`
    * `platforms/qwindows.dll` из `Qt/5.6.3/plugins`
    * `libgcc_s_dw2-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` из `Qt/Tools/mingw492_32/bin`

#### 4. Обновление языковых файлов

~~~
cd .build
update_translations.bat
~~~

* Переменная BUILD_DIR в bat-файле должна указывать на build-директорию, установленную в конфигурации проекта.
* Команду надо выполнять на той же платформе, где происходил препроцессинг CMakeLists.txt.
* Далее файлы .ts редактируются с помощью Linguist из Qt Creator.

#### 5. Сборка release-версии
* Проверить, что в Qt Creator/Kits/Compilers есть компилятор, который использовался для сборки Qt.
* Добавить собранную версию Qt в Qt Creator/Kits/Versions и Qt Creator/Kits/Kits. Версия компилятора должна соответствовать версии, с которой происходила сборка Qt.
* Обновить версию приложения в CMakeLists.txt, пересканировать проект (Rescan project), чтобы версия прописалась в заголовочные файлы.
* Закоммитить изменения.
* Откомпилировать приложение нужной версией Qt.
    * актуализировать значения переменных в `/build/vars-mingw-*.cmd`. 
    * i386 (Windows XP): `.build\build-win-i386.bat`
    * x86_64, MSVC: `.build\build-win-msvc.bat`
    * x86_64, mingw: `.build\build-win-mingw.bat`
* Каждый скрипт конфигурирует, собирает, раскладывает релиз в `.build\release\<имя>\` и сам пакует его в `.build\release\<имя>.zip`.
* Скрипты всегда пересобирают проект инкрементально. Чтобы собрать с нуля, добавьте аргумент `clean`, например `.build\build-win-msvc.bat clean`.
* Загрузить как релиз на GitHub, добавив последнему коммиту тег с номером версии.


---
## macOS

https://doc.qt.io/qt-6/macos.html

Далее описывается установка окружения из offline-инсталляторов, так как сетевая установка под виртуальными машинами работала нестабильно.

### 1. Установить xcode

Дистрибутив взять здесь: https://xcodereleases.com, нужен аккаунт на Apple Developer.

* Скопировать файл `.xip` в папку `/Applications` и там распаковать. Файл `.xip` удалить
* Выполнить команду `sudo xcode-select --switch /Applications/Xcode.app`

### 2. Установить HomeBrew

https://brew.sh/

### 3. Установить утилиты

cmake, ninja, принять лицензию xcode:

```
brew install cmake
brew install ninja
sudo xcodebuild -license
```

### 4. Установить Qt

Если нужна только компиляция, то Qt Creator можно не устанавливать.

С https://download.qt.io/official_releases/qtcreator/latest/ скачать Qt Creator Offline Installer и с https://download.qt.io/official_releases/qt/ Qt Sources (qt-everywhere-src-X.X.X.tar.xz)
* Установить Qt Creator обычным образом (открыть файл `.dmg`, перетащить иконку в `/Applications`).
* qt-everywhere-src-X.X.X.tar.xz поместить в ~/Downloads

### 5. Собрать статическую версию Qt

Для сборки универсальной статической версии Qt x86_64+arm64 используйте скрипт `.build/macos_build_qt_universal.sh`. Данный скрипт исходит из следующих условий:
* Архив с исходными файлами лежит в `~/Downloads/`.
* Распаковка происходит в `/tmp`.
* Установка происходит в `~/Qt-$QT_VERSION-static-universal`.
* После установки можно отдельно скопировать Qt в `/usr/local` и установить системные пути при необходимости.

Перед запуском нужно актуализировать пути в первых строках файла.

После перезагрузки системы `/tmp` очищается, поэтому для повторного запуска надо распаковывать исходники заново.

### 6. Добавить Kit в Qt Creator 

Из папки `~/Qt-$QT_VERSION-static-universal` или `/usr/local/Qt-X.X.X-static` (включить отображение скрытых папок при необходимости).

#### 7. Сборка приложения

Для сборки приложения используется скрипт `.build/build-macos.sh`. Перед первым запуском необходимо актуализировать следующе переменные: QT_PATH.

На выходе должен быть получен файл `.dmg`.

---
## Ubuntu 20.04

В целях совместимости, для сборки выбирается самая старая версия из текущих на поддержке, на 04.2025 это Ubuntu 20.04. В более новых версиях не запустится linuxdeployqt.

#### 1. Установить программы
* https://download.qt.io/, скачать online-инсталлятор (возможно, из России понадобится зарубежный VPN) и установить следующие компоненты:
    * Qt [X.X.X]
        * Desktop
        * Sources 
        * Plugins
            * Qt5Compatibility 
    * Qt Developer and Designer tools
        * Qt Creator
        * cmake
        * ninja
* Компилятор `sudo apt install build-essential`
* Скачать linuxdeployqt: https://github.com/probonopd/linuxdeployqt/releases и разместить в `~/Downloads`.

Добавить в `~/.profile` пути к cmake и ninja:
```
PATH="~/Qt/Tools/Cmake/bin:~/Qt/Tools/Ninja:${PATH}"
```

Если cmake выводит ошибку вида `Qt6Gui could not be found because dependency WrapOpenGL could not be found.`, поставить библиотеку:

```
sudo apt install libgl1-mesa-dev
```

#### 2. Настроить Kit в Qt Creator
* Если нужна полная очистка, удалить файлы __CMakeLists.txt.user*__.

#### 3. Сборка приложения

Для сборки приложения используется скрипт `.build/build-linux.sh`. Перед первым запуском необходимо актуализировать следующе переменные: QT_PATH и LINUXDEPLOYQT.

cmake и ninja должны быть в PATH (см. п. 1).

На выходе должен быть получен файл `.AppImage`.


