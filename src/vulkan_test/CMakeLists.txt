set(PROJECT_NAME vulkan_test)
project(${PROJECT_NAME})

find_package(Vulkan)
find_path(SPIRV_REFLECT_DIR spirv_reflect.h
	HINTS
		"$ENV{VULKAN_SDK}/Source/Spirv-Reflect"
		"${Vulkan_INCLUDE_DIRS}/../Source/Spirv-Reflect"
		"/usr/include/spirv-reflect"
)
if (NOT SPIRV_REFLECT_DIR)
	message(FATAL_ERROR "SPIRV-Reflect was not found. Install the LunarG Vulkan SDK and set VULKAN_SDK.")
endif()


file(GLOB_RECURSE PROJECT_FILES "*.h" "*.hpp" "*.cpp")
if (NOT WIN32)
	list(FILTER PROJECT_FILES EXCLUDE REGEX "/retired_code/")
endif()
# add Spirv-Reflect source files to the project
file(GLOB_RECURSE SPIRV_REFLECT_FILES "${SPIRV_REFLECT_DIR}/*.h" "${SPIRV_REFLECT_DIR}/*.hpp" "${SPIRV_REFLECT_DIR}/*.c" "${SPIRV_REFLECT_DIR}/*.cpp")
message(STATUS ${SPIRV_REFLECT_FILES})

if (WIN32)
	add_executable(${PROJECT_NAME} WIN32 ${PROJECT_FILES} ${SPIRV_REFLECT_FILES})
else()
	find_library(SDL2_LIBRARY SDL2 REQUIRED)
	add_executable(${PROJECT_NAME} ${PROJECT_FILES} ${SPIRV_REFLECT_FILES})
	target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME_DEBUG   "${PROJECT_NAME}_debug"
    OUTPUT_NAME_RELEASE "${PROJECT_NAME}_release"
    OUTPUT_NAME_RELWITHDEBINFO "${PROJECT_NAME}_relwithdebinfo"
    OUTPUT_NAME_MINSIZEREL "${PROJECT_NAME}_minsizerel"
)

target_precompile_headers(${PROJECT_NAME} PRIVATE stdafx.h)
# Tell CMake to treat .c files as C++ files
set_source_files_properties(${SPIRV_REFLECT_FILES} PROPERTIES LANGUAGE CXX)

target_link_libraries(${PROJECT_NAME} common ${Vulkan_LIBRARY})

set(INC_ASSIMP ${CMAKE_SOURCE_DIR}/ext/assimp/include)
include_directories(${CMAKE_SOURCE_DIR}/src 
	${PROJECT_SOURCE_DIR} 
	${INC_ASSIMP}
	${Vulkan_INCLUDE_DIRS}
	${Vulkan_INCLUDE_DIRS}/vma
	${SPIRV_REFLECT_DIR}
	)
	
message(${Vulkan_INCLUDE_DIRS})
message(${Vulkan_LIBRARY})

ADD_DEFINITIONS(-DUNICODE -D_UNICODE -DCOMPILE_CPP -DNOMINMAX)

if (MSVC)	
	set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})
	message(${EXECUTABLE_OUTPUT_PATH})
endif(MSVC)

func_group_src("${PROJECT_FILES} ${SPIRV_REFLECT_FILES}")