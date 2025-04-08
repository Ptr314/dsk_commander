cmake_minimum_required(VERSION 3.21)

project(DISKCommander VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

# Добаввляем библиотеку, но отключаем компиляцию утилит командногй строки
set(ENABLE_DSK_TOOLS OFF CACHE BOOL "Disable command line tools from the library")
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/dsk_tools/)

# Основные файлы с переводами
set(SUPPORTED_LANGUAGES ru en)
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/i18n/en_us.ts
    ${CMAKE_SOURCE_DIR}/i18n/ru_ru.ts
)

# Добаввляем строки из файла конфигурации в файлы для перевода
set(JSON_FILE ${CMAKE_SOURCE_DIR}/resources/files/config.json)
set(QML_OUTPUT ${CMAKE_SOURCE_DIR}/i18n/config.qml)
add_custom_command(
    OUTPUT ${QML_OUTPUT}
    COMMAND python ${CMAKE_SOURCE_DIR}/../.build/extract_json.py ${JSON_FILE} ${QML_OUTPUT}
    DEPENDS ${JSON_FILE}
)
add_custom_target(update_translations
    COMMAND Qt::lupdate ${CMAKE_SOURCE_DIR} ${QML_OUTPUT} -ts ${TS_FILES} -noobsolete
    DEPENDS ${QML_OUTPUT}
)

# Основные файлы проекта
set(PROJECT_SOURCES
        main.cpp
        mainutils.h
        mainwindow.h mainwindow.cpp mainwindow.ui
        viewdialog.h viewdialog.cpp viewdialog.ui
        convertdialog.h convertdialog.cpp convertdialog.ui
        fileparamdialog.h fileparamdialog.cpp
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

set(RESOURCE_FILES
    resources.qrc
    ${CMAKE_BINARY_DIR}/qt_base.qrc
)

# Собираем в зависимости от версии Qt
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    QT6_ADD_RESOURCES(PROJECT_RESOURCES
        ${RESOURCE_FILES}
    )
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${PROJECT_RESOURCES}
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
        file(APPEND ${QRC_FILE} "        <file alias=\"${_qt_id}\">${QM_FILE}</file>\n")
    endforeach()

    file(APPEND ${QRC_FILE} "    </qresource>\n</RCC>\n")

    qt5_add_resources(PROJECT_RESOURCES
        ${RESOURCE_FILES}
        ${QRC_FILE}
    )

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

# Настройки для правильной генерации исполняемого файла под разные платформы

if(APPLE)
    if(${QT_VERSION} VERSION_LESS 6.1.0)
      set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME})
    endif()

    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build universal binary for macOS" FORCE)
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
configure_file (globals.h.in ${CMAKE_CURRENT_SOURCE_DIR}/globals.h @ONLY)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
