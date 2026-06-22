cmake_minimum_required(VERSION 3.20)
project(JezioraPolski-GRK C CXX)

set(CMAKE_CXX_STANDARD 17)

# Optimization flags for Release builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# 1. Include directories for Glad and Homebrew paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(APPLE)
    include_directories(/opt/homebrew/include) # Apple Silicon Homebrew path
    include_directories(/usr/local/include)     # Intel Mac path
    link_directories(/opt/homebrew/lib)
    link_directories(/usr/local/lib)
endif()

# 2. Find GLFW
find_package(glfw3 REQUIRED)

# 3. Define the executable target
set(SRC_FILES
    src/Camera.cpp
    src/Model.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/Utils.cpp
    src/PlantManager.cpp
    src/glad.c
)
add_executable(JezioraPolski-GRK main.cpp ${SRC_FILES})

# 4. Link Libraries depending on the Operating System
if(APPLE)
    target_link_libraries(JezioraPolski-GRK
            glfw
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreVideo"
    )
elseif(WIN32)
    target_link_libraries(JezioraPolski-GRK
            glfw
            opengl32
    )
else() # Linux / other Unix
    target_link_libraries(JezioraPolski-GRK
            glfw
            GL
    )
endif()