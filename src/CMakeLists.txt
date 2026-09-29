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
)
target_include_directories(query_graph
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)


# --------------------- ir_builder ---------------------
add_library(ir_builder
  ir_builder/steps/GlobalEntitiesBuilder.cpp
  ir_builder/steps/RelPredicatesBuilder.cpp
  ir_builder/steps/SignatureBuilder.cpp
  ir_builder/IRBuilder.cpp
  ir_builder/ExpressionRecognition.cpp
  ir_builder/PredicateRegistry.cpp
)

target_include_directories(ir_builder
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(ir_builder
  PUBLIC
    query_graph
)

# --------------------- execution --------------------------
add_library(execution
  execution/Helpers.cpp
  execution/PredicateEvaluator.cpp
  execution/ProcessSingleStrategy.cpp
)

target_include_directories(execution
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(execution
  PUBLIC
    query_graph
    ir_builder
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