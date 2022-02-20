cmake_minimum_required(VERSION 3.5)

FILE(GLOB_RECURSE app_sources "src/*.*")

set(depends
    "LinkedList"        # https://github.com/vortigont/LinkedList
)

# Build ESP32-LightManager as an ESP-IDF component
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${app_sources}
        INCLUDE_DIRS "src"
        REQUIRES ${depends}
        #PRIV_REQUIRES
    )
return()
endif()

project(ESP32-LightManager VERSION 0.1.0)

#add_subdirectory(src)
#target_compile_options(${COMPONENT_TARGET} PRIVATE -fno-rtti)
