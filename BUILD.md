# Настройка окружения и компиляция приложения

Программа написана как проект cmake с использованием библиотеки Qt. Ниже описывается установка окружения под разные ОС и компиляция приложения.

В данный момент программа компилируется под следующие платформы:

* Windows 7+
    * Версия i386 на основе Qt 5.15.2 и mingw 8.1.0.
    * Есть проблемы с отображением на hi-res-мониторах с включенным масштабированием.
* Windows 10+
    * Версия х86_64. Актуальная версия Qt 6.8.2 и mingw 13.10.
* macOS 15 (возможна совместимость с более ранними версиями)
    * Версия х86_64. Qt 6.8.2, xcode 16
* Linux Ubuntu 20.04+
    * Версия х86_64. Qt 6.8.2, gcc 9.4.0

Версия х86_64 для Windows использует статическую компоновку. 

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
        * cmake
        * ninja

#### 2. Настроить debug-версию в Qt Creator
* Если нужна полная очистка, удалить файлы __CMakeLists.txt.user*__.

#### 3. Компиляция статической версии Qt

##### Qt6 (актуальная версия)

https://doc.qt.io/qt-6/windows-building.html

* Отредактировать `.build/vars-mingw-latest.cmd` на действительные пути.
* Открыть командную строку и скомпилировать Qt:

```
cd репозиторий-приложения\.build
%SystemRoot%\system32\cmd.exe /E:ON /V:ON /k vars-mingw-latest.cmd
cd C:\Temp
mkdir qt-build
cd qt-build
configure.bat -static -static-runtime -release -opensource -confirm-license -nomake examples -nomake tests -prefix c:\DEV\Qt\%_QT_VERSION%-static
cmake --build . --parallel
cmake --install .
```

##### Qt5 для Windows 7

Для Windows 7 необходима версия Qt 5.15 и mingw 8.1.0 (https://download.qt.io/archive/qt/5.15/5.15.16/single/)

~~~
cd репозиторий-приложения\.build
%SystemRoot%\system32\cmd.exe /E:ON /V:ON /k vars-mingw-qt5.15.cmd
cd C:\Temp
mkdir qt5.15-build
cd qt5.15-build
configure.bat -static -static-runtime -release -nomake examples -nomake tests -opensource -confirm-license -no-opengl -skip qtlocation -prefix c:\DEV\Qt\%_QT_VERSION%-static
mingw32-make
mingw32-make install
~~~

Примечания: 
* -no-opengl используется для исключения установки OpenGL SDK.
* -skip qtlocation исключает непонятную ошибку компиляции в этом модуле

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
    * i386: `./build/build-win-i386.bat`.
    * x86_64: `./build/build-win-latest.bat`.
* Упаковать директории в `./build/release` в zip.
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


