# 使用 GLOB 递归搜索所有源文件
file(GLOB_RECURSE MAIN_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/**/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.c"
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
        # "main.c"
        "main_factory_test.c"
    INCLUDE_DIRS
        "."
)
