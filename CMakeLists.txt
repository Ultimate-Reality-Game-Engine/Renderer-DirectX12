# CMakeList.txt : UltReality::Rendering::DirectX12Renderer

# CMake configuration and settings ****************************************************************
#**************************************************************************************************

cmake_minimum_required (VERSION 3.25)

project(RENDERER_DIRECTX12 VERSION 1.0.0)

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
	message(FATAL_ERROR "The DirectX12Renderer can only be built to target Windows")
endif()

# Make sure CMake exports its compile commands so things like IntelliSense can detect them in editor
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create options that are dependent onthis project being top level
option(RENDERER_DIRECTX12_INSTALL "Enable project install" ${PROJECT_IS_TOP_LEVEL})

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

# DirectX12 SDK setup
find_path(DirectX12_INCLUDE_DIR "d3d12.h")
find_library(DirectX12_LIB d3d12)

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

# Create list of all source files to be used in this target
file(GLOB_RECURSE DirectX12Renderer_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if(DirectX12Renderer_SOURCE)
# Add DirectX12 Renderer library target
add_library(DirectX12Renderer SHARED ${DirectX12Renderer_SOURCE})

# Connect the Renderers headers to the library
target_include_directories(DirectX12Renderer PUBLIC 
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
 	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # This is used when the library is installed
)

# Link the DirectX12 Helpers, DirectX12, RendererInterface, and DXGI libraries to the target
target_link_libraries(DirectX12Renderer PRIVATE DirectX-Headers RendererInterface ${DirectX12_LIB} ${DXGI_LIB} dxguid)

# Set the RENDERER_INTERFACE_EXPORTS macro for DirectX12Renderer
target_compile_definitions(DirectX12Renderer PRIVATE RENDERER_INTERFACE_EXPORTS)

# Get properties from the RendererInterface library
get_target_property(RendererInterface_VERSION RendererInterface VERSION)
get_target_property(RendererInterface_SOVERSION RendererInterface SOVERSION)

if(${RendererInterface_SOVERSION} GREATER ${PROJECT_VERSION_MAJOR})
	message(SEND_ERROR "DirectX12Renderer must be updated to match the newer major version of RendererInterface\nRendererInterface at version ${RendererInterface_VERSION}\nDirectX12Renderer at version ${PROJECT_VERSION}")
elseif(${PROJECT_IS_TOP_LEVEL})
	message(STATUS "RendererInterface at version ${RendererInterface_VERSION}\n-- DirectX12Renderer at version ${PROJECT_VERSION}")
endif()

set_target_properties(DirectX12Renderer PROPERTIES 
	VERSION ${PROJECT_VERSION} 
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# End Target Creation *****************************************************************************
#**************************************************************************************************

# Installation and Packing Configuration **********************************************************
#**************************************************************************************************
if(RENDERER_DIRECTX12_INSTALL)
	# Install the targets
	install(
		TARGETS DirectX12Renderer 
		EXPORT DirectX12Renderer_Targets 
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # Static libraries/import libraries (.lib files for .dll linking) 
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # Shared libraries (.so) 
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # .exe or .dll 
		PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # Headers/include directories marked as PUBLIC 
		PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # Headers/include directories marked as PRIVATE
	)

	# Create the targets CMake file which contains the above definitions
	install(
		EXPORT DirectX12Renderer_Targets 
		FILE DirectX12Renderer_Targets.cmake 
		NAMESPACE UltReality::Rendering:: 
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/DirectX12Renderer/cmake
	)

	# Install the actual includes
	install(
		DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ 
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

	# Generate and install the package version config files
	include(CMakePackageConfigHelpers)
	write_basic_package_version_file(
		"DirectX12Renderer_ConfigVersion.cmake" 
		VERSION ${PROJECT_VERSION} 
		COMPATIBILITY SameMajorVersion
	)
	configure_package_config_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake_config/DirectX12Renderer_Config.cmake.in" 
		"${CMAKE_CURRENT_BINARY_DIR}/DirectX12Renderer_Config.cmake" 
		INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/DirectX12Renderer/cmake
	)

	# Install the CMake config files
	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/DirectX12Renderer_ConfigVersion.cmake" 
		"${CMAKE_CURRENT_BINARY_DIR}/DirectX12Renderer_Config.cmake" 
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/DirectX12Renderer/cmake
	)

	# Define Package install paths
	set(INCLUDEDIR_FOR_PKG_CONFIG "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
	set(LIBDIR_PKG_CONFIG "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

	# Create and install the package config file
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake_config/DirectX12Renderer.pc.in" 
		"${CMAKE_CURRENT_BINARY_DIR}/DirectX12Renderer.pc" @ONLY
	)

	# Install the package config file
	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/DirectX12Renderer.pc" 
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
	)

	# A version that is often used to denote a specific build of the software, including revisions, builds, or other metadata
	set(PACKAGE_VERSION_BUILD "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")

	set(PACKAGE_VERSION "${PROJECT_VERSION}-${PACKAGE_VERSION_BUILD}")

	set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/out/package")

	set(CPACK_PACKAGE_NAME "DirectX12Renderer")
	set(CPACK_PACKAGE_VERSION "${PACKAGE_VERSION}")

	set(CPACK_PACKAGE_VENDOR "Andrew Todd")
	set(CPACK_PACKAGE_CONTACT "andrewdanieltodd@gmail.com")
	include(CPack)
	if(${PROJECT_IS_TOP_LEVEL})
		message(STATUS "PACKAGE_VERSION is: ${PACKAGE_VERSION}")
		message(STATUS "PACKAGE_FILE_NAME is: ${CPACK_PACKAGE_FILE_NAME}")
	endif()
endif()
endif()

# End Installation and Packing Configuration ******************************************************
#**************************************************************************************************

# Create Unit Test Groups *************************************************************************
#**************************************************************************************************

# Create a set for build all unit tests
set_property(GLOBAL PROPERTY UNIT_TEST_TARGETS "")
set_property(GLOBAL PROPERTY UNIT_TEST_SOURCES "")

get_property(test_targets GLOBAL PROPERTY UNIT_TEST_TARGETS)
if(test_targets)
	# Create a custom target for all tests
	add_custom_target(all_unit_tests DEPENDS ${test_targets})
endif()

get_property(test_source GLOBAL PROPERTY UNIT_TEST_SOURCES)
if(test_source)
	# Create an executable for the custom target, such that the IDEs can see it as a runnable target
	add_executable(run_all_unit_tests EXCLUDE_FROM_ALL ${test_source})
	# Link the executable with GTest and the TokenValueParser library
	target_link_libraries(run_all_unit_tests PRIVATE GTest::gtest_main GenParseBase GenParseCmdLine)
	set_target_properties(run_all_unit_tests PROPERTIES INSTALLABLE OFF)
endif()

# End Create Unit Test Groups *********************************************************************
#**************************************************************************************************