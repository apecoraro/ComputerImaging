project(CS570_DX12)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common.cmake)

add_compile_options(/MP)

set(sources
    Sample.cpp
    Sample.h
    SampleRenderer.cpp
    SampleRenderer.h
    ImageProcessor.h
    ImageProcessor.cpp
    ImageRenderer.h
    ImageRenderer.cpp
    stdafx.cpp
    stdafx.h)

set(shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageProcessor.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderImage.hlsl)

set(config
	${CMAKE_CURRENT_SOURCE_DIR}/../Common/SampleSettings.json
)

source_group("Sources" FILES ${sources})
source_group("Shaders" FILES ${shaders})
source_group("Config" FILES ${config})

copyCommand("${shaders}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX)
copyCommand("${config}" ${CMAKE_HOME_DIRECTORY}/bin)

add_executable(${PROJECT_NAME} WIN32 ${sources} ${shaders} ${config})

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC Cauldron_DX12 ImGUI amd_ags DXC d3dcompiler D3D12)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../glm)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../Common)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")
set_source_files_properties(${shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")

addManifest(${PROJECT_NAME})
