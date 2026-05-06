# SPDX-FileCopyrightText: None
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.16)

set(QT_MIN_VERSION "6.5.0")
set(KF6_MIN_VERSION "6.0.0")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(kTile VERSION 0.1.0 LANGUAGES CXX)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS
    Core
    Gui
    Qml
    Quick
    DBus
)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    Config
    CoreAddons
    KCMUtils
)

add_subdirectory(kcm)
add_subdirectory(session-helper)

# KWin/Script package (same layout as kpackagetool6 -i kwin-script)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/"
    DESTINATION "${KDE_INSTALL_DATADIR}/kwin/scripts/org.kde.ktile"
)
