file(GLOB_RECURSE ALL_FILES "*.hlsl" "*.h")
file(GLOB_RECURSE CS_FILES "*.cs.hlsl")
# for raster shaders
file(GLOB_RECURSE RAS_FILES "*.ras.hlsl")

file(GLOB_RECURSE RAY_FILES "*.ray.hlsl")
file(GLOB_RECURSE INC_FILES "*.inc.hlsl")

macro(Gen_output_files src_files stage_suffix)
	foreach(var ${src_files})
		get_filename_component(file_wle ${var} NAME_WLE)
		get_filename_component(file_wle ${file_wle} NAME_WLE)
		get_filename_component(file_dir ${var} DIRECTORY)
		
		file(RELATIVE_PATH rel_dir "${CMAKE_CURRENT_SOURCE_DIR}" "${file_dir}")

		cmake_path(APPEND OUTPUT_FILENAME ${EXECUTABLE_OUTPUT_PATH} ${PROJECT_NAME} ${rel_dir} ${file_wle})
		#message(" TEST ${OUTPUT_FILENAME_TEST}")

		#set(OUTPUT_FILENAME ${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME}/${file_wle})
		list(APPEND OUTPUT_FILES ${OUTPUT_FILENAME}.${stage_suffix}.obj ${OUTPUT_FILENAME}.${stage_suffix}.asm)
	endforeach(var)
endmacro()

# DXC compiling
# https://simoncoenen.com/blog/programming/graphics/DxcCompiling	
macro(Dxc_compile src_files dxc_path profile entry_point stage_suffix spirv_opt)
	foreach(var ${src_files})
		get_filename_component(file_wle ${var} NAME_WLE)
		# remove .ras extension for raster shaders, replace with .vs/.ps depending on the profile
		get_filename_component(file_wle ${file_wle} NAME_WLE)

		get_filename_component(file_dir ${var} DIRECTORY)
		file(RELATIVE_PATH rel_dir "${CMAKE_CURRENT_SOURCE_DIR}" "${file_dir}")
		cmake_path(APPEND OUTPUT_FILENAME ${EXECUTABLE_OUTPUT_PATH} ${PROJECT_NAME} ${rel_dir} ${file_wle})
		
		#set(OUTPUT_FILENAME ${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME}/${file_wle})
		
		add_custom_command(
			OUTPUT ${OUTPUT_FILENAME}.${stage_suffix}.obj ${OUTPUT_FILENAME}.${stage_suffix}.asm 
			COMMAND ${dxc_path} -T ${profile} -E ${entry_point} ${spirv_opt} -Ni -Od -Fo ${OUTPUT_FILENAME}.${stage_suffix}.obj -Fc ${OUTPUT_FILENAME}.${stage_suffix}.asm -Zpr ${var}
			COMMAND echo Effect Compiler ${var}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			#MAIN_DEPENDENCY ${var}
			DEPENDS ${var}
			COMMENT "DXC Compile... ${var} -> ${OUTPUT_FILENAME}.${stage_suffix}.obj"
			)
		#message(${FXC} /T "vs_5_0" /Ni /Od /Fo ${OUTPUT_FILENAME}.obj /Fc ${OUTPUT_FILENAME}.asm ${var} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
	endforeach(var)
endmacro()

function(filter_files_with_entry out_var entry_name)
    set(result)
    foreach(f IN LISTS ARGN)
        file(READ "${f}" src_text)
        string(REGEX MATCH "(^|[^A-Za-z0-9_])${entry_name}[ \t\r\n]*\\(" has_entry "${src_text}")
        if(has_entry)
            list(APPEND result "${f}")
        endif()
    endforeach()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

set(PROJECT_NAME shader_dxil)
project(${PROJECT_NAME})

filter_files_with_entry(RAS_VS_FILES "vsmain" ${RAS_FILES})
filter_files_with_entry(RAS_PS_FILES "psmain" ${RAS_FILES})
filter_files_with_entry(RAS_MS_FILES "msmain" ${RAS_FILES})

set(OUTPUT_FILES)
Gen_output_files("${RAS_VS_FILES}" "vs")
Gen_output_files("${RAS_PS_FILES}" "ps")
Gen_output_files("${RAS_MS_FILES}" "ms")
Gen_output_files("${CS_FILES}" "cs")
Gen_output_files("${RAY_FILES}" "ray")
message(${OUTPUT_FILES})

add_custom_target(${PROJECT_NAME} SOURCES ${ALL_FILES} DEPENDS ${OUTPUT_FILES})
set_source_files_properties(${ALL_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")

#find_program(FXC fxc
#	PATHS "$ENV{PROGRAMFILES\(X86\)}/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
#set(DXC "${CMAKE_SOURCE_DIR}/ext/dxc/dxc.exe")
# use dxc from the Vulkan SDK, which is more up to date than the one in Windows SDK
find_package(Vulkan)
set(DXC "${Vulkan_INCLUDE_DIRS}/../bin/dxc.exe")

file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME}
		PRE_BUILD 
		COMMAND del *.* /q
		COMMAND echo Cleaning files
		WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME}
		VERBATIM
		)
#message("Cleaning files{\"\\n\", s8b}")

Dxc_compile("${RAS_VS_FILES}" ${DXC} "vs_6_0" "vsmain" "vs" "")
Dxc_compile("${RAS_PS_FILES}" ${DXC} "ps_6_0" "psmain" "ps" "")
Dxc_compile("${RAS_MS_FILES}" ${DXC} "ms_6_5" "msmain" "ms" "")
Dxc_compile("${CS_FILES}" ${DXC} "cs_6_0" "csmain" "cs" "")
Dxc_compile("${RAY_FILES}" ${DXC} "lib_6_5" "raymain" "ray" "")

func_group_src("${ALL_FILES}")

############################################
#### Project Begin
set(PROJECT_NAME shader_spirv)
project(${PROJECT_NAME})

filter_files_with_entry(RAS_VS_FILES "vsmain" ${RAS_FILES})
filter_files_with_entry(RAS_PS_FILES "psmain" ${RAS_FILES})
filter_files_with_entry(RAS_MS_FILES "msmain" ${RAS_FILES})

set(OUTPUT_FILES)
Gen_output_files("${RAS_VS_FILES}" "vs")
Gen_output_files("${RAS_PS_FILES}" "ps")
Gen_output_files("${RAS_MS_FILES}" "ms")
Gen_output_files("${CS_FILES}" "cs")
Gen_output_files("${RAY_FILES}" "ray")
message(${OUTPUT_FILES})

add_custom_target(${PROJECT_NAME} SOURCES ${ALL_FILES} DEPENDS ${OUTPUT_FILES})

set_source_files_properties(${ALL_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")

find_package(Vulkan)
set(DXC "${Vulkan_INCLUDE_DIRS}/../bin/dxc.exe")


file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME}
		PRE_BUILD 
		COMMAND del *.* /q
		COMMAND echo Cleaning files
		WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME}
		VERBATIM
		)
		
#set(SPIRV_OPTS "-spirv -fspv-reflect -fspv-target-env=\"vulkan1.1spirv1.4\"")
set(SPIRV_OPTS "-spirv -fspv-target-env=\"vulkan1.1spirv1.4\"")
message(${SPIRV_OPTS})

Dxc_compile("${RAS_VS_FILES}" ${DXC} "vs_6_0" "vsmain" "vs" ${SPIRV_OPTS} )
Dxc_compile("${RAS_PS_FILES}" ${DXC} "ps_6_0" "psmain" "ps" ${SPIRV_OPTS} )
Dxc_compile("${RAS_MS_FILES}" ${DXC} "ms_6_5" "msmain" "ms" ${SPIRV_OPTS} )
Dxc_compile("${CS_FILES}" ${DXC} "cs_6_0" "csmain" "cs" ${SPIRV_OPTS} )
Dxc_compile("${RAY_FILES}" ${DXC} "lib_6_5" "raymain" "ray" ${SPIRV_OPTS} )

func_group_src("${ALL_FILES}")
