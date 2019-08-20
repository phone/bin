cmake_minimum_required(VERSION 3.8)

set(CMAKE_CXX_STANDARD 14)

find_package(wxWidgets REQUIRED all)
include(${wxWidgets_USE_FILE})
include_directories(${wxWidgets_INCLUDE_DIRS})

add_executable(xchoose xchoose.cpp)

target_link_libraries(xchoose
	${wxWidgets_LIBRARIES})
