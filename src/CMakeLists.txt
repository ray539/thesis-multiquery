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
  kernel/CachedDatabaseAccess.cpp
  kernel/InMemoryDatabase.cpp
  kernel/Printer.cpp
)
target_include_directories(kernel
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(kernel
  PUBLIC
    common
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
target_link_libraries(query_graph
  PUBLIC
    common
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
    common
    query_graph
)

# --------------------- execution --------------------------
add_library(execution
  execution/with_shared_blocks/DFSHelper.cpp
  execution/with_shared_blocks/Eval.cpp
  execution/with_shared_blocks/SharedBlock.cpp
  execution/EagerExecutor.cpp
  execution/PredicateEvaluator.cpp
  execution/CandidateSpace.cpp
  execution/Printer.cpp
)

target_include_directories(execution
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(execution
  PUBLIC
    common
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
    query_graph
    kernel
    execution
)

# --------------------- evaluation executable
add_executable(evaluation
  evaluation/6.3/Test.cpp
  evaluation/6.4/Test.cpp
  evaluation/6.5/Test.cpp
  evaluation/6.6/Test.cpp
  evaluation/6.7/Test.cpp
  evaluation/Main.cpp
  evaluation/DataGraphGen.cpp
  evaluation/QueryGraphGen.cpp
  evaluation/ChaoticDataGraphGenerator.cpp
  evaluation/PlantedMotifGenerator.cpp
  evaluation/SharedBlockGenerator.cpp
)

target_include_directories(evaluation
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(evaluation
  PRIVATE
    kernel
    query_graph
    ir_builder
    execution
)