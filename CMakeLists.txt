cmake_minimum_required(VERSION 3.16)
project(MicroserviceTemplate VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_DOCS "Build documentation" OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find spdlog
find_package(spdlog REQUIRED)

# Find nlohmann/json
find_package(nlohmann_json REQUIRED)

# httplib and Paho MQTT are installed system-wide, no find_package needed

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(/usr/local/include)

# Add subdirectories
add_subdirectory(src)

# Install configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MicroserviceTemplateConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Export targets
install(TARGETS microservice_template
    EXPORT MicroserviceTemplateTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MicroserviceTemplateConfigVersion.cmake"
    DESTINATION lib/cmake/MicroserviceTemplate
)

install(EXPORT MicroserviceTemplateTargets
    FILE MicroserviceTemplateTargets.cmake
    NAMESPACE MicroserviceTemplate::
    DESTINATION lib/cmake/MicroserviceTemplate
)
