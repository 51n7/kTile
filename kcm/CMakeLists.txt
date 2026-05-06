# SPDX-License-Identifier: BSD-2-Clause

kcmutils_add_qml_kcm(kcm_ktile
    INSTALL_NAMESPACE plasma/kcms/systemsettings
    SOURCES
        kcmktile.cpp
        kcmktile.h
)
target_link_libraries(kcm_ktile PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::DBus
    KF6::I18n
    KF6::ConfigCore
    KF6::KCMUtilsCore
    KF6::KCMUtils
    KF6::KCMUtilsQuick
    KF6::CoreAddons
)
