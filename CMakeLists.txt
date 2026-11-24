# ---------------------------------------------------------------------------
#  Анимиран текстуриран хеликоптер - курсов проект по Компютърна графика
#  Сборка: CMake 3.16+, C++17, OpenGL 3.3 Core, GLFW 3.4, GLM 1.0.1
#  GLFW и GLM се изтеглят автоматично от GitHub (FetchContent).
#  glad (OpenGL loader) е генериран предварително и е в external/glad.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(HelicopterCG LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- GLFW: прозорец, OpenGL контекст, клавиатура и мишка -------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE)

# --- GLM: векторна и матрична математика ------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(glfw glm)

# --- glad: зарежда адресите на OpenGL функциите -----------------------------
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)

# --- Самото приложение -------------------------------------------------------
add_executable(Helicopter
    src/main.cpp
    src/Shader.cpp
    src/Mesh.cpp
    src/ProceduralTextures.cpp
    src/SceneNode.cpp
    src/Helicopter.cpp
    src/Camera.cpp
    src/World.cpp
    src/Renderer.cpp
    src/Hud.cpp)

target_link_libraries(Helicopter PRIVATE glad glfw glm::glm)

if (MSVC)
    target_compile_options(Helicopter PRIVATE /W3 /utf-8)
    # Visual Studio да стартира проекта Helicopter по подразбиране
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Helicopter)
else()
    target_compile_options(Helicopter PRIVATE -Wall -Wextra)
endif()
