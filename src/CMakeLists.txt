# Source directory CMakeLists.txt for Microservice Template

# Collect all service source files
file(GLOB SERVICE_SOURCES
    services/*.cpp
)

# Collect all utility source files
file(GLOB UTIL_SOURCES
    utils/*.cpp
)

# Collect all core source files (for custom business logic)
file(GLOB CORE_SOURCES
    core/*.cpp
)

# Add Process Manager sources explicitly
list(APPEND CORE_SOURCES
    core/process_manager.cpp
    core/process_manager_api.cpp
)

# Create the main executable
add_executable(microservice_template 
    main.cpp
    ${SERVICE_SOURCES}
    ${UTIL_SOURCES}
    ${CORE_SOURCES}
)

# Set target properties
set_target_properties(microservice_template PROPERTIES
    OUTPUT_NAME "microservice_template"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Link libraries
target_link_libraries(microservice_template 
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    pthread
    paho-mqttpp3
    paho-mqtt3a
)

# Include directories
target_include_directories(microservice_template PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    /usr/local/include
)

# Install targets
install(TARGETS microservice_template
    RUNTIME DESTINATION bin
)
