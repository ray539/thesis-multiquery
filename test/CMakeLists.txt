# ------------------------- ir_builder test
add_executable(ir_builder_test
  ir_builder_test/main.cpp
)
target_include_directories(ir_builder_test
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ir_builder_test
  PRIVATE
    query_graph
    ir_builder
)

# --------------------- kernel_test
add_executable(kernel_test
  kernel_test/main.cpp
)
target_include_directories(kernel_test
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(kernel_test
  PRIVATE
    query_graph
    kernel
)
