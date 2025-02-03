cmake_minimum_required(VERSION 3.16)

project(DISKCommander VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

add_subdirectory(${CMAKE_SOURCE_DIR}/libs/dsk_tools/)

set(JSON_FILE ${CMAKE_SOURCE_DIR}/../deploy/config.json)
set(QML_OUTPUT ${CMAKE_BINARY_DIR}/config.qml)

set(TS_FILES
    ${CMAKE_SOURCE_DIR}/i18n/en_us.ts
    ${CMAKE_SOURCE_DIR}/i18n/ru_ru.ts
)

add_custom_command(
    OUTPUT ${QML_OUTPUT}
    COMMAND python ${CMAKE_SOURCE_DIR}/../.build/extract_json.py ${JSON_FILE} ${QML_OUTPUT}
    DEPENDS ${JSON_FILE}
)

add_custom_target(update_translations
    COMMAND Qt::lupdate ${CMAKE_SOURCE_DIR} ${QML_OUTPUT} -ts ${TS_FILES}
    DEPENDS ${QML_OUTPUT}
)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.h mainwindow.cpp mainwindow.ui
        viewdialog.h viewdialog.cpp viewdialog.ui
        charmaps.h
        ../deploy/config.json
        ../README.md
        ../LINKS.md
        ${TS_FILES}
)

set(RESOURCE_FILES
    resources.qrc
)

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
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
    )

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/dsk_tools/include/
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    dsk_tools
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

configure_file(../deploy/dsk_com.ini ${CMAKE_BINARY_DIR}/dsk_com.ini COPYONLY)
configure_file(../deploy/config.json ${CMAKE_BINARY_DIR}/config.json COPYONLY)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
