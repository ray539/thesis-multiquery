# ---------------------- kernel -------------------------------
add_library(kernel
  kernel/InMemoryDatabase.cpp
)
target_include_directories(kernel
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# ---------------------- query_graph ----------------------
add_library(query_graph
  query_graph/QueryGraph.cpp
  query_graph/QueryBuilder.cpp
)
target_include_directories(query_graph
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)



# --------------------- execution --------------------------
add_library(execution
  execution/ExecutionStrat1.cpp
)
target_include_directories(execution
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(execution
  PUBLIC
    query_graph
    kernel
)

# --------------------- parser -----------------------------
add_library(parser
  parser/CypherParser.cpp
)
target_include_directories(parser
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(parser
  PUBLIC
    query_graph
)


# --------------------- apps -----------------------------------
# runnable executables
add_executable(main
  apps/Main.cpp
)

target_link_libraries(main
  PRIVATE
    parser
    query_graph
    kernel
    execution
)

# ---------------------- compiler options --------------------
# make it as strict as possible to catch bugs
if (MSVC)
  target_compile_options(main PRIVATE /W4 /permissive-)
else()
  target_compile_options(main PRIVATE
    -Wall -Wextra -Winit-self -Wuninitialized -Wreturn-type
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -g
  )
  target_link_options(main PRIVATE
    -fsanitize=address,undefined
  )
endif()