# SPDX-License-Identifier: BSD-2-Clause

add_executable(ktile-session-helper main.cpp)
target_link_libraries(ktile-session-helper PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
)
install(TARGETS ktile-session-helper DESTINATION ${KDE_INSTALL_BINDIR})
install(
    FILES ktile-session-helper.desktop
    DESTINATION ${KDE_INSTALL_AUTOSTARTDIR}
)
