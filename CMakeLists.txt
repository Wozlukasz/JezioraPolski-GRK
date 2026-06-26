cmake_minimum_required(VERSION 3.20)
project(JezioraPolski-GRK C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for Release builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# 1. Include local GLFW directory
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")

add_subdirectory(extern/glfw)

# 2. Define the executable target (Cleaned from duplicates)
set(SRC_FILES
        main.cpp
        src/Camera.cpp
        src/Model.cpp
        src/Shader.cpp
        src/Texture.cpp
        src/Utils.cpp
        src/PlantManager.cpp
        src/BubbleSystem.cpp
        src/FishManager.cpp
        src/CubemapGenerator.cpp
        src/EncyclopediaOverlay.cpp
        src/glad.c
)

add_executable(JezioraPolski-GRK ${SRC_FILES})
add_executable(find_shore find_shore.cpp)

# 3. Modern Include Directories (Target-based)
target_include_directories(JezioraPolski-GRK PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/glm
)

if(APPLE)
    target_include_directories(JezioraPolski-GRK PRIVATE /opt/homebrew/include /usr/local/include)
    target_link_directories(JezioraPolski-GRK PRIVATE /opt/homebrew/lib /usr/local/lib)
endif()

# 4. Link Libraries depending on the Operating System
if(APPLE)
    target_link_libraries(JezioraPolski-GRK PRIVATE
            glfw
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreVideo"
    )
elseif(WIN32)
    target_link_libraries(JezioraPolski-GRK PRIVATE
            glfw
            opengl32
    )
else() # Linux / other Unix
    target_link_libraries(JezioraPolski-GRK PRIVATE
            glfw
            GL
    )
endif()