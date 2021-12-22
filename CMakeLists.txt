# CMake minimum version
cmake_minimum_required(VERSION 3.7)

# Create project
project(WhistleDetector
    VERSION 0.1
    LANGUAGES CXX
)

# Set C++ standard and preprocessor macros
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(APP_NAME="${CMAKE_PROJECT_NAME}")
add_compile_definitions(APP_VERSION="${CMAKE_PROJECT_VERSION}")

# Find and configure Qt5 and fftw3
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 COMPONENTS Core Multimedia Network REQUIRED)

# Add executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/WhistleDetector)
add_executable(WhistleDetector
    src/main.cpp
    src/application.h  src/application.cpp
    src/detector.h     src/detector.cpp
    src/settings.h     src/settings.cpp
)

# Add and link dependencies
add_subdirectory(qmqtt)
target_include_directories(WhistleDetector PRIVATE $ENV{CONDA_PREFIX}/include)
target_link_directories(WhistleDetector PRIVATE $ENV{CONDA_PREFIX}/lib)
#message("hihi $ENV{CONDA_PREFIX}")
target_link_libraries(WhistleDetector PRIVATE Qt5::Core Qt5::Multimedia Qt5::Network qmqtt fftw3)

# Deploy dll's with executable on Windows
if (WIN32)
    get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    # Call windeployqt
    add_custom_command(TARGET WhistleDetector POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
                --verbose 1
                --no-compiler-runtime # MSVC: vc_redist, GCC/MinGW: compiler dll's
                --no-angle            # Angle = portable OpenGL
                --no-opengl-sw        # No OpenGL was used!
                "$<TARGET_FILE:WhistleDetector>"
    )
endif()

# Deploy configuration and icons
add_custom_command(TARGET WhistleDetector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/config.ini"
        "$<TARGET_FILE_DIR:WhistleDetector>"
)
