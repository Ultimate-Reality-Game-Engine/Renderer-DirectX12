# CMakeList.txt : RenderEngineProj::Renderer_DirectX12, include source and define
# project specific logic here.

# CMake configuration and settings ****************************************************************
#**************************************************************************************************

cmake_minimum_required (VERSION 3.25)

project(RENDERER_DIRECTX12 VERSION 1.0.0)

enable_testing()

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Google tests require mimimum c++ version 14
# setting minimum c++ version to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Fetch the RendererInterface library
FetchContent_Declare(
	RendererInterface 
	GIT_REPOSITORY https://github.com/Ultimate-Reality-Game-Engine/Renderer-Interface.git 
	GIT_TAG latest
)
FetchContent_MakeAvailable(RendererInterface)

# Create list of all source files to be used in this target
file(GLOB_RECURSE DirectX12Renderer_SOURCE "${CMAKE_SOURCE_DIR}/src/*.cpp")

if(DirectX12Renderer_SOURCE)
# Add DirectX12 Renderer library target
add_library(DirectX12Renderer SHARED ${DirectX12Renderer_SOURCE})

# Connect the Renderers headers to the library
target_include_directories(DirectX12Renderer PUBLIC 
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> 
 	$<INSTALL_INTERFACE:include> # This is used when the library is installed
)

# Connect the DirectX headers to the library
target_include_directories(DirectX12Renderer PRIVATE ${DirectX12_INCLUDE_DIR})

# Link the DirectX12, DXC, and DXGI libraries to the target
target_link_libraries(DirectX12Renderer PRIVATE RendererInterface ${DirectX12_LIB} ${DXGI_LIB})

# Get properties from the RendererInterface library
get_target_property(RendererInterface_VERSION RendererInterface VERSION)
get_target_property(RendererInterface_SOVERSION RendererInterface SOVERSION)

if(${RendererInterface_SOVERSION} GREATER ${PROJECT_VERSION_MAJOR})
	message(SEND_ERROR "DirectX12Renderer must be updated to match the newer major version of RendererInterface\nRendererInterface at version ${RendererInterface_VERSION}\nDirectX12Renderer at version ${PROJECT_VERSION}")
else()
	message(STATUS "RendererInterface at version ${RendererInterface_VERSION}\nDirectX12Renderer at version ${PROJECT_VERSION}")
endif()

set_target_properties(DirectX12Renderer PROPERTIES 
	VERSION ${PROJECT_VERSION} 
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# End Target Creation *****************************************************************************
#**************************************************************************************************

# Installation and Packing Configuration **********************************************************
#**************************************************************************************************

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_BINARY_DIR}/RendererDirectX12ConfigVersion.cmake"
	VERSION ${PACKAGE_VERSION}
	COMPATIBILITY AnyNewerVersion
)

# Install the library, header files, and config version file
install(
	TARGETS DirectX12Renderer 
	EXPORT DirectX12RendererTargets
)
install(
	EXPORT DirectX12RendererTargets 
	FILE DirectX12RendererTargets.cmake 
	DESTINATION lib/cmake/DirectX12Renderer
)
install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/DirectX12RendererConfigVersion.cmake" 
	DESTINATION lib/cmake/DirectX12Renderer
)
install(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include
)

install(FILES "${CMAKE_BINARY_DIR}/DirectX12RendererConfigVersion.cmake" DESTINATION "lib/cmake/DirectX12Renderer")

# A version that is often used to denote a specific build of the software, including revisions, builds, or other metadata
set(PACKAGE_VERSION_BUILD "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")

set(PACKAGE_VERSION "${PROJECT_VERSION}-${PACKAGE_VERSION_BUILD}")

set(PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/out/package")
file(MAKE_DIRECTORY "${PACKAGE_DIRECTORY}")

set(CPACK_PACKAGE_NAME "DirectX12Renderer")
set(CPACK_PACKAGE_VERSION "${PACKAGE_VERSION}")

set(CPACK_PACKAGE_VENDOR "Andrew Todd")
set(CPACK_PACKAGE_CONTACT "andrewdanieltodd@gmail.com")
include(CPack)
message(STATUS "PACKAGE_VERSION is: ${PACKAGE_VERSION}")
message(STATUS "PACKAGE_FILE_NAME is: ${CPACK_PACKAGE_FILE_NAME}")
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