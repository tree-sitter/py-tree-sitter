cmake_minimum_required(VERSION 3.15)
project(tree_sitter LANGUAGES C)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(tree-sitter REQUIRED)

Python3_add_library(_binding MODULE WITH_SOABI
    tree_sitter/binding/language.c
    tree_sitter/binding/lookahead_iterator.c
    tree_sitter/binding/node.c
    tree_sitter/binding/parser.c
    tree_sitter/binding/point.c
    tree_sitter/binding/query.c
    tree_sitter/binding/query_cursor.c
    tree_sitter/binding/query_predicates.c
    tree_sitter/binding/range.c
    tree_sitter/binding/tree.c
    tree_sitter/binding/tree_cursor.c
    tree_sitter/binding/module.c
)

target_include_directories(_binding PRIVATE
    tree_sitter/binding
)

target_link_libraries(_binding PRIVATE tree-sitter::tree-sitter)

target_compile_definitions(_binding PRIVATE
    _POSIX_C_SOURCE=200112L
    _DEFAULT_SOURCE
    PY_SSIZE_T_CLEAN
    "PY_TS_VERSION=\"${PY_TS_VERSION}\""
)

if(MSVC)
    target_compile_options(_binding PRIVATE /std:c11 /wd4244)
else()
    target_compile_options(_binding PRIVATE
        -std=c11
        -fvisibility=hidden
        -Wno-cast-function-type
        -Werror=implicit-function-declaration
    )
    # FIXME: GCC optimizer bug workaround for #330 & #386
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
        target_compile_options(_binding PRIVATE --param=early-inlining-insns=9)
    endif()
endif()

if(APPLE)
    set_target_properties(_binding PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif()

install(TARGETS _binding DESTINATION tree_sitter)
