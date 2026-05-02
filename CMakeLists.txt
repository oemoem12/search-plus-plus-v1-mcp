cmake_minimum_required(VERSION 3.21)
project(mcpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  # Try common system paths
  find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS
      /usr/include
      /usr/local/include
      ENV CONDA_PREFIX/include
      /home/cat/miniconda3/include
    PATH_SUFFIXES include
  )
  if(NLOHMANN_JSON_INCLUDE_DIR)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
    )
  else()
    message(FATAL_ERROR "nlohmann/json not found. Install it or set CMAKE_PREFIX_PATH.")
  endif()
endif()

# Find libcurl for HTTP requests
find_package(CURL REQUIRED)
if(CURL_FOUND)
  message(STATUS "Found libcurl: ${CURL_LIBRARIES}")
  message(STATUS "libcurl include: ${CURL_INCLUDE_DIRS}")
endif()

set(MCPP_SOURCES
  src/Types.cpp
  src/Transport.cpp
  src/JsonRpc.cpp
  src/Server.cpp
  src/Tool.cpp
  src/Resource.cpp
  src/Prompt.cpp
  src/SearchEngine.cpp
)

add_library(mcpp ${MCPP_SOURCES})
target_include_directories(mcpp PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(mcpp PUBLIC nlohmann_json::nlohmann_json ${CURL_LIBRARIES})
target_include_directories(mcpp PRIVATE ${CURL_INCLUDE_DIRS})
target_compile_options(mcpp PRIVATE -Wall -Wextra -Wpedantic)

add_executable(mcpp_example examples/main.cpp)
target_link_libraries(mcpp_example PRIVATE mcpp)

install(TARGETS mcpp
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
