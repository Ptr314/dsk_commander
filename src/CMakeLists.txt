cmake_minimum_required(VERSION 3.21)

# Read version from VERSION file at project root
file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" PROJECT_VERSION_STRING)
string(STRIP "${PROJECT_VERSION_STRING}" PROJECT_VERSION_STRING)
string(REPLACE "." ";" VERSION_LIST ${PROJECT_VERSION_STRING})
list(GET VERSION_LIST 0 PROJECT_VERSION_MAJOR)
list(GET VERSION_LIST 1 PROJECT_VERSION_MINOR)
list(GET VERSION_LIST 2 PROJECT_VERSION_PATCH)

# Настройки macOS должны быть заданы ДО project(): иначе они не попадут ни в
# проверки компилятора, ни в подключаемую ниже библиотеку dsk_tools -- та
# собиралась бы под архитектуру и минимальную версию системы по умолчанию.
# Без FORCE, чтобы значения можно было переопределить из командной строки.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build universal binary for macOS")

# Enable OBJCXX on macOS for host_helpers.cpp Objective-C++ compilation
if(APPLE)
    project(DISKCommander VERSION ${PROJECT_VERSION_STRING} LANGUAGES CXX OBJCXX)
else()
    project(DISKCommander VERSION ${PROJECT_VERSION_STRING} LANGUAGES CXX)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Оптимизация размера исполняемого файла
#
#   DC_OPTIMIZE_SIZE  -- -O2 вместо -O3, раскладка кода и данных по отдельным
#                        секциям и последующая вычистка неиспользуемого
#                        линкером (--gc-sections / -dead_strip / /OPT:REF),
#                        удаление таблицы символов из релизной сборки.
#   DC_LTO            -- межмодульная оптимизация (LTO/IPO). Заметно
#                        уменьшает размер, но требует больше памяти и времени
#                        на линковке. Автоматически отключается там, где
#                        тулчейн её не поддерживает.
#
# Флаги задаются на уровне директории, поэтому распространяются и на
# подключаемую ниже библиотеку dsk_tools.
# ---------------------------------------------------------------------------
option(DC_OPTIMIZE_SIZE "Optimize the release binary for size" ON)
option(DC_LTO           "Enable link-time optimization (LTO/IPO) for release builds" ON)

# Для single-config генераторов без явно заданного типа сборки берём Release:
# иначе молча собирается вариант вообще без оптимизации.
get_property(_dc_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _dc_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Конфигурации, для которых применяются оптимизации размера
set(_dc_release_cfg "$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>")

if(DC_OPTIMIZE_SIZE)
    if(MSVC)
        # /Gy, /Gw -- отдельная COMDAT-секция на каждую функцию и на каждую
        # глобальную переменную; /OPT:REF выбрасывает неиспользуемые,
        # /OPT:ICF склеивает одинаковые.
        add_compile_options(
            $<${_dc_release_cfg}:/Gy>
            $<${_dc_release_cfg}:/Gw>
        )
        add_link_options(
            $<${_dc_release_cfg}:/OPT:REF>
            $<${_dc_release_cfg}:/OPT:ICF>
            $<${_dc_release_cfg}:/INCREMENTAL:NO>
        )
    else()
        # -O3 разворачивает циклы и агрессивно инлайнит, что для этого
        # приложения даёт лишние сотни килобайт без заметного выигрыша:
        # тяжёлых вычислительных циклов здесь нет, всё упирается в ввод-вывод.
        string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

        add_compile_options(
            $<${_dc_release_cfg}:-ffunction-sections>
            $<${_dc_release_cfg}:-fdata-sections>
        )
        if(APPLE)
            add_link_options(
                $<${_dc_release_cfg}:LINKER:-dead_strip>
                $<${_dc_release_cfg}:LINKER:-x>
            )
        else()
            add_link_options(
                $<${_dc_release_cfg}:LINKER:--gc-sections>
                $<${_dc_release_cfg}:LINKER:-s>
            )
        endif()
    endif()
endif()

if(DC_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _dc_lto_supported OUTPUT _dc_lto_error LANGUAGES CXX)
    # LTO в mingw 4.9 (сборка под Windows XP) приводит к ошибкам линковки,
    # поэтому для старых gcc её не включаем.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8)
        set(_dc_lto_supported OFF)
        set(_dc_lto_error "gcc < 8")
    endif()
    if(_dc_lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
    else()
        message(STATUS "DC: LTO disabled (${_dc_lto_error})")
    endif()
endif()


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

# ---------------------------------------------------------------------------
# Статическая сборка Qt
#
# При статической сборке плагины Qt линкуются внутрь исполняемого файла,
# и рядом с ним не нужны ни Qt*.dll, ни каталоги platforms/ и styles/.
#
# Если Qt собран с -static-runtime, то рантайм компилятора тоже должен
# линковаться статически. Qt выставляет это самостоятельно, но только для целей,
# созданных через qt_add_executable()/qt_add_library(). Библиотека dsk_tools --
# обычный add_library(), поэтому без явной установки ниже она собирается с /MD
# и линковка падает на LNK2038 (RuntimeLibrary mismatch).
#
# Установка должна выполняться ДО создания целей, т.е. до add_subdirectory().
# ---------------------------------------------------------------------------
get_target_property(_dc_qt_core_type Qt${QT_VERSION_MAJOR}::Core TYPE)
if(_dc_qt_core_type STREQUAL "STATIC_LIBRARY")
    set(DC_QT_STATIC ON)
    message(STATUS "DC: static Qt detected, no Qt runtime DLLs are required")
else()
    set(DC_QT_STATIC OFF)
endif()

if(DC_QT_STATIC AND QT_FEATURE_static_runtime)
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(MINGW)
        add_link_options(-static)
    endif()
endif()

# Добавляем библиотеку, но отключаем компиляцию утилит командной строки
set(ENABLE_DSK_TOOLS OFF CACHE BOOL "Disable command line tools from the library")
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/dsk_tools/)

# Основные файлы с переводами
set(SUPPORTED_LANGUAGES ru en)
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/i18n/en_us.ts
    ${CMAKE_SOURCE_DIR}/i18n/ru_ru.ts
)

# Добавляем строки из файла конфигурации в файлы для перевода
set(JSON_FILE ${CMAKE_SOURCE_DIR}/resources/files/config.json)
set(EXTRACT_SCRIPT ${CMAKE_SOURCE_DIR}/../.build/extract_json.py)
set(QML_OUTPUT ${CMAKE_SOURCE_DIR}/i18n/config.qml)
add_custom_command(
    OUTPUT ${QML_OUTPUT}
    COMMAND python ${EXTRACT_SCRIPT} ${JSON_FILE} ${QML_OUTPUT}
    DEPENDS ${JSON_FILE} ${EXTRACT_SCRIPT}
)
add_custom_target(update_translations
    COMMAND Qt::lupdate ${CMAKE_SOURCE_DIR} ${QML_OUTPUT} -ts ${TS_FILES} -noobsolete
    DEPENDS ${QML_OUTPUT}
)

# Основные файлы проекта
set(PROJECT_SOURCES
        main.cpp
        mainutils.h                 mainutils.cpp
        FileOperations.h            FileOperations.cpp
        placeholders.h
        mainwindow.h                mainwindow.cpp
        viewdialog.h                viewdialog.cpp          viewdialog.ui
        explorerdialog.h            explorerdialog.cpp      explorerdialog.ui
        convertdialog.h             convertdialog.cpp       convertdialog.ui
        fileparamdialog.h           fileparamdialog.cpp
        formatdialog.h              formatdialog.cpp
        FilePanel.cpp               FilePanel.h
        FileTable.cpp               FileTable.h
        thememanager.cpp            thememanager.h
        aboutdlg.ui
        fileinfodialog.ui
        resources/files/config.json
        resources/files/Info.plist
        ../README.md
        ../LINKS.md
        ../MANUAL.md
        ../BUILD.md
)

# Добавляем переводы интерфейса из Qt
find_package(Qt${QT_VERSION_MAJOR} REQUIRED Core)
if(NOT ${Qt6Core_DIR} STREQUAL "")
    set(QT_TRANSLATIONS_DIR "${Qt6Core_DIR}/../../../translations")
else()
    set(QT_TRANSLATIONS_DIR "${Qt5Core_DIR}/../../../translations")
endif()

set(QT_TRANSLATION_FILES "")
foreach(LANG ${SUPPORTED_LANGUAGES})
    set(QT_TRANSLATION_FILE "${QT_TRANSLATIONS_DIR}/qtbase_${LANG}.qm")
    if(EXISTS ${QT_TRANSLATION_FILE})
        list(APPEND QT_TRANSLATION_FILES ${QT_TRANSLATION_FILE})
    else()
        message(WARNING "Файл перевода Qt (qtbase_${LANG}.qm) не найден: ${QT_TRANSLATION_FILE}")
    endif()
endforeach()

if(QT_TRANSLATION_FILES STREQUAL "")
    message(FATAL_ERROR "Не найдено ни одного файла перевода Qt! Проверьте установку.")
endif()

file(WRITE ${CMAKE_BINARY_DIR}/qt_base.qrc "<RCC>\n  <qresource prefix=\"/i18n\">\n")
foreach(FILE ${QT_TRANSLATION_FILES})
    get_filename_component(FILENAME ${FILE} NAME)
    file(APPEND ${CMAKE_BINARY_DIR}/qt_base.qrc "    <file alias=\"${FILENAME}\">${FILE}</file>\n")
endforeach()
file(APPEND ${CMAKE_BINARY_DIR}/qt_base.qrc "  </qresource>\n</RCC>\n")

# Собираем в зависимости от версии Qt
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    QT6_ADD_RESOURCES(MAIN_RESOURCES resources.qrc)
    QT6_ADD_RESOURCES(QTBASE_RESOURCES ${CMAKE_BINARY_DIR}/qt_base.qrc)

    # config.json is embedded via resources.qrc. The .qrc macro form does not
    # track the *contents* of the listed files, so editing config.json alone
    # would not re-run rcc. Append it as an explicit dependency of the rcc step.
    add_custom_command(OUTPUT ${MAIN_RESOURCES} APPEND DEPENDS ${JSON_FILE})

    set(PROJECT_RESOURCES ${MAIN_RESOURCES} ${QTBASE_RESOURCES})

    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${PROJECT_RESOURCES}
        resources/files/basic.css
        resources/files/basic_dark.css
        resources/files/stylesheet.qss
        resources/files/stylesheet_dark.qss
    )

    qt_add_translations(${PROJECT_NAME} TS_FILES
        ${TS_FILES}
    )
else()
    # В пятой версии приходится генерировать qm вручную, класть их в отдельный файл ресурса
    # и подключать этот файл в проект
    find_program(LRELEASE_EXECUTABLE lrelease)

    set(QRC_FILE ${CMAKE_BINARY_DIR}/translations.qrc)
    file(WRITE ${QRC_FILE} "<RCC>\n    <qresource prefix=\"/i18n\">\n")

    foreach(_ts_file ${TS_FILES})
        get_filename_component(_qt_id ${_ts_file} NAME_WE)
        set(QM_FILE ${CMAKE_BINARY_DIR}/${_qt_id}.qm)
        execute_process(COMMAND ${LRELEASE_EXECUTABLE} ${_ts_file} -qm  ${QM_FILE})
        file(APPEND ${QRC_FILE} "        <file alias=\"${_qt_id}.qm\">${QM_FILE}</file>\n")
    endforeach()

    file(APPEND ${QRC_FILE} "    </qresource>\n</RCC>\n")

    qt5_add_resources(MAIN_RESOURCES resources.qrc)
    qt5_add_resources(OTHER_RESOURCES ${CMAKE_BINARY_DIR}/qt_base.qrc ${QRC_FILE})

    # See the Qt6 branch: force rcc to re-run when config.json changes.
    add_custom_command(OUTPUT ${MAIN_RESOURCES} APPEND DEPENDS ${JSON_FILE})

    set(PROJECT_RESOURCES ${MAIN_RESOURCES} ${OTHER_RESOURCES})

    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        ${PROJECT_RESOURCES}
    )
endif()

# Подключаем библиотеку
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/dsk_tools/include/
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    dsk_tools
)

# При статической сборке в exe по умолчанию попадают ВСЕ плагины подключённых
# модулей. Приложение работает только с PNG (поддержка встроена в QtGui, отдельный
# плагин не нужен), в файлы картинки не сохраняются, сеть и SQL не
# используются -- лишние группы плагинов исключаем.
if(DC_QT_STATIC AND ${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_import_plugins(${PROJECT_NAME}
        EXCLUDE_BY_TYPE imageformats
        EXCLUDE_BY_TYPE iconengines
        EXCLUDE_BY_TYPE sqldrivers
        EXCLUDE_BY_TYPE networkinformation
        EXCLUDE_BY_TYPE tls
        EXCLUDE_BY_TYPE qmltooling
        EXCLUDE_BY_TYPE printsupport
    )
endif()

# Настройки для правильной генерации исполняемого файла под разные платформы

if(APPLE)
    if(${QT_VERSION} VERSION_LESS 6.1.0)
      set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME})
    endif()

    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/files/DISKCommander.icns")
    set(PLIST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/files/Info.plist")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE "DISKCommander.icns"
        MACOSX_BUNDLE_INFO_PLIST "${PLIST_FILE}"
    )
  target_sources(${PROJECT_NAME} PRIVATE ${ICON_FILE})
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_FILE}" "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/"
  )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

# С помощью этого файла передаем номер версии приложения
set(CMAKE_CONFIGURE_FILE_CONTENT_NEWLINE_STYLE UNIX)
configure_file (globals.h.in ${CMAKE_CURRENT_SOURCE_DIR}/globals.h @ONLY)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

# if(WIN32)
#     # Get Qt paths
#     get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
#     get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

#     # Use windeployqt for automatic Qt deployment
#     add_custom_command(TARGET DISKCommander POST_BUILD
#             COMMAND "${QT_BIN_DIR}/windeployqt.exe"
#             --no-compiler-runtime
#             "$<TARGET_FILE:DISKCommander>"
#             COMMENT "Running windeployqt..."
#     )
# endif()
