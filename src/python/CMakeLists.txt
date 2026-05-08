find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX') or '.so')"
    OUTPUT_VARIABLE PYTHON_EXTENSION_SUFFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(TARGET cmf_backtester)
add_library(${TARGET} MODULE PyBacktester.cpp)
add_dependencies(${TARGET} pybind11-headers)
target_link_libraries(${TARGET} PRIVATE
    hft-market-making-backtest
    pybind11-headers
    Python3::Module
)
set_target_properties(${TARGET} PROPERTIES
    PREFIX ""
    SUFFIX "${PYTHON_EXTENSION_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python
)
