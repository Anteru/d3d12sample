CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
PROJECT(ANTERU_D3D12_SAMPLE)
ADD_SUBDIRECTORY(extern)

 SET(SOURCES
  src/D3D12Sample.cpp

  src/ImageIO.cpp
  src/Utility.cpp
  src/Window.cpp
  )

SET(HEADERS
  inc/D3D12Sample.h

  inc/ImageIO.h
  inc/Utility.h
  inc/Window.h

  ${CMAKE_CURRENT_BINARY_DIR}/shaders.h
  ${CMAKE_CURRENT_BINARY_DIR}/sample_texture.h)

FIND_PACKAGE(PythonInterp 3.4)
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders.h
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_HOME_DIRECTORY}/tools/sourceToHeader.py
		${CMAKE_CURRENT_SOURCE_DIR}/src/Shaders.hlsl
		SampleShaders
		> ${CMAKE_CURRENT_BINARY_DIR}/shaders.h
	DEPENDS
src/Shaders.hlsl)
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sample_texture.h
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_HOME_DIRECTORY}/tools/binaryToHeader.py
		${CMAKE_CURRENT_SOURCE_DIR}/src/anteru-new.png
		SampleTexture
		> ${CMAKE_CURRENT_BINARY_DIR}/sample_texture.h
	DEPENDS
src/anteru-new.png)

ADD_EXECUTABLE(anD3D12Sample ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(anD3D12Sample d3dx12 d3dcompiler dxgi d3d12)
TARGET_INCLUDE_DIRECTORIES(anD3D12Sample
	PUBLIC inc
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
