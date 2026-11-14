# Optimizing Multi-Query Subgraph Matching for Cypher

This repository is based on my undergraduate thesis, **Optimizing Multi-Query Subgraph Matching for Cypher**,  with the same name. 

The project is a standalone research implementation rather than an extension to Neo4j itself. Query graphs and data graphs are represented using lightweight in-memory structures so that the matching algorithms can be evaluated independently.

## Requirements

The project is written in C++23.

Recommended environment:

- Linux or WSL;
- GCC 15 / `g++-15`;
- CMake 3.28 or newer.

GCC with libstdc++ is recommended rather than another compiler/toolchain.

In particular, the implementation uses C++23 library features such as `std::generator` and also uses the GNU-specific header:

```cpp
#include <bits/stdc++.h>
```

As a result, portability to MSVC or non-libstdc++ environments is not currently a goal of the project.

GCC 14 and newer versions of libstdc++ provide `std::generator`, but GCC 15 is the recommended compiler for reproducing the project.

No external graph database or third-party library is required.

## Building

Clone the repository and enter the project directory.

Configure the project using GCC 15:

```bash
cd <project root directory>
cmake -B build
```

Then build all targets:

```bash
cmake --build build
```

The root CMake configuration enables C++23 and, on non-MSVC builds, enables AddressSanitizer and UndefinedBehaviorSanitizer together with compiler warnings and debug information.

The main executables are generated under the build directory:

```text
build/src/main
build/src/evaluation

build/test/ir_builder_test
build/test/kernel_test
build/test/shared_block_test
```

For example:

```bash
./build/src/main
```

## Repository structure

The project is divided into four main implementation layers:

```text
QueryGraph
    |
    v
IRBuilder
    |
    v
BuildResult / relationship-oriented IR
    |
    v
Execution
    |
    v
In-memory Kernel
```

The main source directories are under `src/`, while the corresponding public interfaces and shared data structures are under `include/common/`.

### `src/query_graph`

Defines and constructs the input query representation.

A `QueryGraph` contains:

- pattern nodes
- pattern relationships
- query-local variables
- expressions in the form of an expression arena and `ExprId` references
- predicates
- optional shared-block annotations.


This layer represents the query before relationship predicates and graph structure have been canonicalized for multi-query matching. It is designed to be the first intermediate representation obtained right after the parsing stage. The expressions supported are a superset of rcypher.

Currently we do not have a parser. It is recommended that query graphs are built with the `QueryGraphBuilder`. See `test/ir_builder_test/main.cpp` for examples.

Note that consistent with `rcypher`, all entity properties have type `int`.

### `src/ir_builder`

Compiles one or more `QueryGraph`s into the relationship-oriented intermediate representation used by the execution layer.

Its main responsibilities include:

- assigning global identities to corresponding query entities
- recognizing supported expressions
- converting query predicates into canonical forms
- interning equivalent predicates in a `PredicateRegistry`
- constructing relationship edge graphs
- recursively constructing hierarchical predicate signatures
- identifying and compiling shared blocks; an
- producing collapsed edge graphs containing shared-block references

The primary output is `BuildResult`, which contains:

- the compiled edge graph for each query;
- the shared `PredicateRegistry`;
- predicate signatures at each hierarchical level; and
- the shared-block registry.

The shared-block compilation passes are located under:

```text
src/ir_builder/steps/shared_blocks/
```

### `src/kernel`

Implements the in-memory data graph used by the evaluation environment.

The kernel contains representations for data nodes and relationships together with operations needed by the matcher, including:

- scanning relationships
- traversing incoming and outgoing relationships
- accessing relationship endpoints
- reading node and relationship properties
- checking node labels
- checking relationship types
- obtaining node degree information.

`CachedDatabaseAccess` provides cached access to these operations, to simulate database caching.

The kernel also records metrics such as cursor operations and cache accesses so that execution strategies can be compared experimentally.

This layer is deliberately small and is intended to approximate the database-access operations required by the matching algorithms rather than implement a complete graph database.

### `src/execution`

Contains the query-matching algorithms.

The execution layer includes:

- predicate evaluation
- hierarchical neighbourhood filtering
- candidate-set construction
- candidate-space adjacency construction
- eager enumeration
- lazy enumeration
- relationship ordering strategies
- relationship injectivity checks
- shared-block enumeration and caching
- execution metrics.

Two main execution approaches are implemented.

#### Eager execution

`EagerExecutor` precomputes candidate sets and candidate-space adjacency information before enumeration.

It supports several ordering strategies and uses the precomputed candidate space to restrict the relationships considered during backtracking.

#### Lazy execution

`LazyExecutorWithSB` discovers candidates during enumeration rather than constructing the complete candidate space beforehand.

It also supports shared-block execution. Shared-block results are lazily generated and cached using the block identity and matched boundary relationship, allowing the same internal block matches to be reused across different outer search states.

## Evaluation

Experimental code is located under:

```text
src/evaluation/
```

The numbered directories correspond directly to the evaluation sections in the thesis:

```text
src/evaluation/6.3/
src/evaluation/6.4/
src/evaluation/6.5/
src/evaluation/6.6/
src/evaluation/6.7/
```

This is intentional: an experiment appearing in a particular thesis section can be found in the directory with the corresponding section number.

The evaluation code also contains reusable data-graph and query-graph generators, including random, chaotic, planted-motif, and shared-block stress workloads.

All of the evaluation sources are compiled into:

```text
build/src/evaluation
```

`src/evaluation/Main.cpp` is the entry point. To run a particular thesis experiment, invoke the corresponding evaluation function from `main()` and rebuild the `evaluation` target.

For example, the source contains evaluation functions such as:

```cpp
evaluation::candidate_set_evaluation_layers();
evaluation::hp_neighbor_condition();
evaluation::ordering_techniques();
evaluation::shared_block_eval();
```

After changing the selected experiment:

```bash
cmake --build build --target evaluation
./build/src/evaluation
```

## Tests

Currently not all of them are assert based. Due to time constraints, most of them just print out the output objects for the reader to debug.
