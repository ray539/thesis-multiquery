cmake_minimum_required(VERSION 3.28)

project(
  multiquery
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # so vscode intellisense keeps up


# ---------------------- compiler options --------------------
# make it as strict as possible to catch bugs
if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(
    -Wall -Wextra -Winit-self -Wuninitialized -Wreturn-type
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -g
  )
  add_link_options(
    -fsanitize=address,undefined
  )
endif()

add_subdirectory(src)
add_subdirectory(test)
