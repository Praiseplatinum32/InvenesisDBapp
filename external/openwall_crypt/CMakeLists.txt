# OpenWall Crypt Library
cmake_minimum_required(VERSION 3.16)

# Define the openwall_crypt static library
add_library(openwall_crypt STATIC
    crypt_blowfish.c
    crypt_gensalt.c
    wrapper.c
    x86.S
)

# Set target properties
set_target_properties(openwall_crypt PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# Include directories for the library
target_include_directories(openwall_crypt
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# Export the target for use by other modules
target_compile_definitions(openwall_crypt
    PUBLIC CRYPT_BLOWFISH_H
)

# Platform-specific configurations
if(WIN32)
    # Windows-specific settings if needed
    target_compile_definitions(openwall_crypt PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if(UNIX)
    # Unix-specific settings if needed
    target_compile_definitions(openwall_crypt PRIVATE _GNU_SOURCE)
endif()