# ---------------------- common ----------------
add_library(common
  common/Common.cpp
)
target_include_directories(common
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

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
  query_graph/Printer.cpp
  query_graph/QueryGraphBuilder.cpp
)
target_include_directories(query_graph
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)


# --------------------- ir_builder ---------------------
add_library(ir_builder
  ir_builder/steps/shared_blocks/AnalyseBlocks.cpp
  ir_builder/steps/shared_blocks/BuildCollapsedEdgeGraph.cpp
  ir_builder/steps/shared_blocks/BuildRawEdgeGraph.cpp
  ir_builder/steps/shared_blocks/MakeSharedBlock.cpp
  ir_builder/steps/shared_blocks/SharedBlockEdgeGraphBuilder.cpp
  ir_builder/steps/GlobalEntitiesBuilder.cpp
  ir_builder/steps/RelPredicatesBuilder.cpp
  ir_builder/steps/SignatureBuilder.cpp
  ir_builder/IRBuilder.cpp
  ir_builder/ExpressionRecognition.cpp
  ir_builder/PredicateRegistry.cpp
  ir_builder/Printer.cpp
  ir_builder/InternalPrinter.cpp
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
  execution/with_shared_blocks/DFSHelper.cpp
  execution/with_shared_blocks/Eval.cpp
  execution/with_shared_blocks/SharedBlock.cpp
  execution/PredicateEvaluator.cpp
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


# # --------------------- parser -----------------------------
# add_library(parser
#   parser/CypherParser.cpp
# )
# target_include_directories(parser
#   PUBLIC
#     ${PROJECT_SOURCE_DIR}/include
# )
# target_link_libraries(parser
#   PUBLIC
#     query_graph
# )



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

