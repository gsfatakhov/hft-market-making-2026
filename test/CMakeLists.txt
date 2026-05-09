include(CTest)

if (EXISTS ${CMAKE_SOURCE_DIR}/3rdparty/Catch2/extras/Catch.cmake)
    include(${CMAKE_SOURCE_DIR}/3rdparty/Catch2/extras/Catch.cmake)
else()
    function(catch_discover_tests TARGET)
    # Dummy macro to satisfy CMake until it fetches Catch2 repo.
    endfunction()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/test)

file(GLOB TEST_SRC CONFIGURE_DEPENDS *.cpp)

# executable
set(TARGET hft-market-making-tests)
add_executable(${TARGET} ${TEST_SRC})

# Link against the necessary libraries
target_link_libraries(${TARGET} PRIVATE
    Catch2-static-lib
    hft-market-making-common
    hft-market-making-backtest
)

# Register the tests with CTest
catch_discover_tests(${TARGET})

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME python-api-smoke
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/PythonApiSmokeTest.py
)
set_tests_properties(python-api-smoke PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
)

add_test(NAME python-avellaneda-stoikov
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/PythonAvellanedaStoikovTest.py
)
set_tests_properties(python-avellaneda-stoikov PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/python:${CMAKE_SOURCE_DIR}/strategies"
)

add_test(NAME python-avellaneda-stoikov-microprice
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/PythonAvellanedaStoikovMicropriceTest.py
)
set_tests_properties(python-avellaneda-stoikov-microprice PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/python:${CMAKE_SOURCE_DIR}/strategies"
)
