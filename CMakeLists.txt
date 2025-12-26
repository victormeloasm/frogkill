cmake_minimum_required(VERSION 3.22)
project(frogkill VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt6 Widgets app
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_standard_project_setup()

add_executable(frogkill
    src/main.cpp
    src/app_controller.cpp
    src/app_controller.h
    src/main_window.cpp
    src/main_window.h
    src/process_model.cpp
    src/process_model.h
    src/procfs.cpp
    src/procfs.h
    src/system_sampler.cpp
    src/system_sampler.h
    src/util.cpp
    src/util.h
)

target_link_libraries(frogkill PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
target_compile_definitions(frogkill PRIVATE FROGKILL_HELPER_PATH="/usr/libexec/frogkill-helper")

add_executable(frogkill-helper
    helper/main.cpp
)

# Helper is intentionally tiny and does not depend on Qt.
target_compile_options(frogkill-helper PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS frogkill RUNTIME DESTINATION bin)

# On Ubuntu and most distros, /usr/libexec is the conventional place for pkexec helpers.
# This install destination is absolute on purpose (requires sudo during install).
install(TARGETS frogkill-helper RUNTIME DESTINATION /usr/libexec)

# Polkit action file for pkexec.
install(FILES install/polkit/org.frogkill.helper.policy DESTINATION /usr/share/polkit-1/actions)

# Convenience files (not installed automatically because they are per-user).
install(FILES install/systemd-user/frogkill.service DESTINATION share/frogkill OPTIONAL)
install(FILES install/autostart/frogkill.desktop DESTINATION share/frogkill OPTIONAL)
