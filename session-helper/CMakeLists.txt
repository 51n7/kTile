# SPDX-License-Identifier: BSD-2-Clause

qt_add_executable(ktile-session-helper
    main.cpp
    drawregioncontroller.cpp
    drawregioncontroller.h
    regionpickercontroller.cpp
    regionpickercontroller.h
    qml.qrc
)

target_link_libraries(ktile-session-helper PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

install(TARGETS ktile-session-helper DESTINATION ${KDE_INSTALL_BINDIR})

configure_file(
    org.kde.ktile.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.ktile.service
    @ONLY
)

install(
    FILES ktile-session-helper.desktop
    DESTINATION ${KDE_INSTALL_AUTOSTARTDIR}
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.ktile.service
    DESTINATION ${KDE_INSTALL_DATADIR}/dbus-1/services
)
