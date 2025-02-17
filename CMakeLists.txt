# CMakeList.txt : UltReality::Rendering::D3D12Renderer

# CMake configuration and settings ****************************************************************
#**************************************************************************************************

cmake_minimum_required (VERSION 3.25)

project(RENDERER_D3D12 VERSION 1.0.0)

# Google tests require mimimum c++ version 14
# setting minimum c++ version to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Define custom configuration types for Debug, Release, and Release-Dev
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "List of supported build modes." FORCE)

# Define compiler definitions to create OS specific code segments
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_compile_definitions(TARGET_OS=Windows _WIN_TARGET)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_compile_definitions(TARGET_OS=Linux _LINUX_TARGET)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin") #MacOS
	add_compile_definitions(TARGET_OS=MacOS _MACOS_TARGET)
endif()

# Ensure that the target OS is Windows
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	message(FATAL_ERROR "The D3D12Renderer can only be built to target Windows")
endif()

# Make sure CMake exports its compile commands so things like IntelliSense can detect them in editor
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create options that are dependent onthis project being top level
option(D3D12_RENDERER_VERBOSE "Enable verbose messages for D3D12Renderer" ${PROJECT_IS_TOP_LEVEL})
option(D3D12_RENDERER_BUILD_TESTS "Build the test suit" ${PROJECT_IS_TOP_LEVEL})

message(STATUS "D3D12_RENDERER_VERBOSE: ${D3D12_RENDERER_VERBOSE}")

include(GNUInstallDirs)

# End CMake configuration and settings ************************************************************
#**************************************************************************************************

# Generated source ********************************************************************************
#**************************************************************************************************

# Create CMake referecne to a directory for configuration templates
set(CONFIG_TEMPLATES_DIR ${CMAKE_SOURCE_DIR}/config_templates)

# Add preprocessor definitions for conditional compilation based on configuration
#add_compile_definitions($<$<CONFIG:Debug>:_DEBUG> $<$<CONFIG:RelWithDebInfo>:_RELEASE_DEV> $<$<CONFIG:Release>:_RELEASE>)

# Create CMake reference to a directory for CMake generated files
set(GENERATED_DIR ${CMAKE_SOURCE_DIR}/generated)
# Create CMake reference to generated directory sub-directories for headers and source files
set(GENERATED_INCLUDE_DIR ${GENERATED_DIR}/include)
set(GENERATED_SRC_DIR ${GENERATED_DIR}/src)

# Create the generated directory and sub-directory structure if it doesn't exist
file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})
file(MAKE_DIRECTORY ${GENERATED_SRC_DIR})

# Function to process the configuration templates and generate source
function(generate_from_template TEMPLATE_DIR OUTPUT_DIR FILE_TYPE)
	file(GLOB TEMPLATES "${TEMPLATE_DIR}/*.in")
	foreach(TEMPLATE ${TEMPLATES})
		get_filename_component(FILENAME ${TEMPLATE} NAME_WE)
		set(OUTPUT_FILE ${OUTPUT_DIR}/${FILENAME}.${FILE_TYPE})
		configure_file(${TEMPLATE} ${OUTPUT_FILE} @ONLY)
	endforeach()
endfunction()

# Generate headers and source files from templates
generate_from_template(${CONFIG_TEMPLATES_DIR}/include ${GENERATED_INCLUDE_DIR} h)
generate_from_template(${CONFIG_TEMPLATES_DIR}/src ${GENERATED_SRC_DIR} cpp)

# End Generated Source ****************************************************************************
#**************************************************************************************************

# Target Creation *********************************************************************************
#**************************************************************************************************

# D3D12 SDK setup
find_path(D3D12_INCLUDE_DIR "d3d12.h")
find_library(D3D12_LIB d3d12)

#[[
# DirectX Shader Compiler (for HLSL) setup
find_path(DXC_INCLUDE_DIR "dxcapi.h")
find_library(DXC_LIB dxc)
]]

# DirectX Graphics Infastructure setup
find_library(DXGI_LIB dxgi)

include(FetchContent)

# Fetch the DirectX headers
FetchContent_Declare(
	DirectX-Headers 
	GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git 
	GIT_TAG v1.614.1
)

if (NOT TARGET RendererInterface)
	# Fetch the RendererInterface library
	FetchContent_Declare(
		RenderingCore 
		GIT_REPOSITORY https://github.com/Ultimate-Reality-Game-Engine/Rendering_Core.git 
		GIT_TAG master
	)
	FetchContent_MakeAvailable(DirectX-Headers RenderingCore)
else()
	FetchContent_MakeAvailable(DirectX-Headers)
endif()

option(D3D12_RENDERER_DEBUG "Enable CMake related debug messages" ON)

file(GLOB_RECURSE D3D12Renderer_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/*/src/*.cpp")
file(GLOB D3D12Renderer_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

#message(STATUS "D3D12Renderer_SOURCE: ${D3D12Renderer_SOURCE}")
#message(STATUS "D3D12Renderer_DIRS: ${D3D12Renderer_DIRS}")

if(NOT DEFINED D3D12Renderer_BUILD)
	set(D3D12Renderer_BUILD ON)
endif()

if(D3D12Renderer_BUILD)
	# Create the UtilitiesStatic target
	if(D3D12Renderer_SOURCE)
		# Add D3D12 Renderer library target
		add_library(D3D12Renderer SHARED ${D3D12Renderer_SOURCE})

		# Set up include directories for the library target
		foreach(dir ${D3D12Renderer_DIRS})
			if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include")
				if(D3D12_RENDERER_DEBUG)	
					message(STATUS "Adding include directory: ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include")
				endif()
				target_include_directories(D3D12Renderer PUBLIC 
					$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include> 
 					$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/D3D12Renderer/${dir}> # This is used when the library is installed
				)
			endif()

			if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/inl")
				if(D3D12_RENDERER_DEBUG)	
					message(STATUS "Adding inl directory: ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/inl")
				endif()
				target_include_directories(D3D12Renderer PUBLIC 
					$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${dir}/inl> 
 					$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/D3D12Renderer/${dir}> # This is used when the library is installed
				)
			endif()
		endforeach()

		# Link the D3D12 Helpers, D3D12, RendererInterface, and DXGI libraries to the target
		target_link_libraries(D3D12Renderer PRIVATE DirectX-Headers RendererInterface ${D3D12_LIB} ${DXGI_LIB} dxguid)

		# Set the RENDERER_INTERFACE_EXPORTS macro for D3D12Renderer
		target_compile_definitions(D3D12Renderer PRIVATE RENDERER_INTERFACE_EXPORTS)

		# Get properties from the RendererInterface library
		get_target_property(RendererInterface_VERSION RendererInterface VERSION)
		get_target_property(RendererInterface_SOVERSION RendererInterface SOVERSION)

		if(${RendererInterface_SOVERSION} GREATER ${PROJECT_VERSION_MAJOR})
			message(SEND_ERROR "D3D12Renderer must be updated to match the newer major version of RendererInterface\n-- RendererInterface at version ${RendererInterface_VERSION}\n-- D3D12Renderer at version ${PROJECT_VERSION}")
		elseif(D3D12_RENDERER_VERBOSE)
			message(STATUS "RendererInterface at version ${RendererInterface_VERSION}\n-- D3D12Renderer at version ${PROJECT_VERSION}")
		endif()

		set_target_properties(D3D12Renderer PROPERTIES 
			VERSION ${PROJECT_VERSION} 
			SOVERSION ${PROJECT_VERSION_MAJOR}
		)
	endif()
endif()

# End Target Creation *****************************************************************************
#**************************************************************************************************

# Installation and Packing Configuration **********************************************************
#**************************************************************************************************

# Install the targets
install(
	TARGETS D3D12Renderer 
	EXPORT D3D12Renderer_Targets 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # Static libraries/import libraries (.lib files for .dll linking) 
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # Shared libraries (.so) 
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # .exe or .dll 
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # Headers/include directories marked as PUBLIC 
	PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # Headers/include directories marked as PRIVATE
)

# Create the targets CMake file which contains the above definitions
install(
	EXPORT D3D12Renderer_Targets 
	FILE D3D12Renderer_Targets.cmake 
	NAMESPACE UltReality::Rendering:: 
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/D3D12Renderer
)

# Install the actual includes
foreach(dir ${D3D12Renderer_DIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include")
        install(
            DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/D3D12Renderer/${dir}"
        )
    endif()

	if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/inl")
        install(
            DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/inl/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/D3D12Renderer/${dir}"
        )
    endif()
endforeach()

# Generate and install the package version config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"D3D12Renderer_ConfigVersion.cmake" 
	VERSION ${PROJECT_VERSION} 
	COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_config/D3D12Renderer_Config.cmake.in" 
	"${CMAKE_CURRENT_BINARY_DIR}/D3D12Renderer_Config.cmake" 
	INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/D3D12Renderer
)

# Install the CMake config files
install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/D3D12Renderer_ConfigVersion.cmake" 
	"${CMAKE_CURRENT_BINARY_DIR}/D3D12Renderer_Config.cmake" 
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/D3D12Renderer
)

# Define Package install paths
set(INCLUDEDIR_FOR_PKG_CONFIG "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
set(LIBDIR_PKG_CONFIG "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# Create and install the package config file
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_config/D3D12Renderer.pc.in" 
	"${CMAKE_CURRENT_BINARY_DIR}/D3D12Renderer.pc" @ONLY
)

# Install the package config file
install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/D3D12Renderer.pc" 
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# A version that is often used to denote a specific build of the software, including revisions, builds, or other metadata
set(PACKAGE_VERSION_BUILD "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")

set(PACKAGE_VERSION "${PROJECT_VERSION}-${PACKAGE_VERSION_BUILD}")

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/out/package")

set(CPACK_PACKAGE_NAME "D3D12Renderer")
set(CPACK_PACKAGE_VERSION "${PACKAGE_VERSION}")

set(CPACK_PACKAGE_VENDOR "Andrew Todd")
set(CPACK_PACKAGE_CONTACT "andrewdanieltodd@gmail.com")
include(CPack)
if(D3D12_RENDERER_VERBOSE)
	message(STATUS "PACKAGE_VERSION is: ${PACKAGE_VERSION}")
	message(STATUS "PACKAGE_FILE_NAME is: ${CPACK_PACKAGE_FILE_NAME}")
endif()

# End Installation and Packing Configuration ******************************************************
#**************************************************************************************************

# Create Unit Test Groups *************************************************************************
#**************************************************************************************************
if (D3D12_RENDERER_BUILD_TESTS)
	if(D3D12_RENDERER_DEBUG)
		message(STATUS "Building test suit for D3D12 Renderer")
	endif()

	set(D3D12Renderer_TEST_DIRS "")

	foreach(dir ${D3D12Renderer_DIRS})
		if(IS_DIRECTORY "${dir}/tests")
			list(APPEND D3D12Renderer_TEST_DIRS "${dir}/tests")
		endif()
		if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/tests")
				if(D3D12_RENDERER_DEBUG)	
					message(STATUS "Adding test directory: ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/tests")
				endif()
				list(APPEND D3D12Renderer_TEST_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/tests")
			endif()
	endforeach()

	# Do not install GTest when packaging Utilities targets
	set(INSTALL_GTEST OFF)
	
	# Add all the tests directories
	foreach(tests_dir ${D3D12Renderer_TEST_DIRS})
		if(D3D12_RENDERER_DEBUG)
			message(STATUS "Adding Sub-Directory: ${tests_dir}")
		endif()
		add_subdirectory("${tests_dir}")
	endforeach()
endif()
# End Create Unit Test Groups *********************************************************************
#**************************************************************************************************