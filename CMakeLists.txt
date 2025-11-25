cmake_minimum_required(VERSION 3.16)
project(TCPChatServer LANGUAGES CXX)

# ==================== CONFIGURAÇÕES GERAIS ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Flags de compilação
add_compile_options(-Wall -Wextra -Wpedantic)

# Modo Release por padrão
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ==================== DIRETÓRIOS DE INCLUDE ====================
include_directories(
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/client
)

# ==================== FONTES COMUNS ====================
set(COMMON_SOURCES
    common/protocol.cpp
    common/socket_utils.cpp
)

# ==================== EXECUTÁVEL DO SERVIDOR ====================
set(SERVER_SOURCES
    server/main.cpp
    server/server.cpp
    server/command_handler.cpp
)

add_executable(server
    ${COMMON_SOURCES}
    ${SERVER_SOURCES}
)

target_link_libraries(server pthread)

# ==================== EXECUTÁVEL DO CLIENTE ====================
set(CLIENT_SOURCES
    client/main.cpp
    client/client.cpp
    client/interface.cpp
)

add_executable(client
    ${COMMON_SOURCES}
    ${CLIENT_SOURCES}
)

target_link_libraries(client pthread)

# ==================== PROPRIEDADES DOS EXECUTÁVEIS ====================
set_target_properties(server client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ==================== INSTALAÇÃO (OPCIONAL) ====================
install(TARGETS server client
    RUNTIME DESTINATION bin
)

# ==================== MENSAGENS DE STATUS ====================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  TCP Chat Server - Configuração")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Output directory: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "")
message(STATUS "Alvos disponíveis:")
message(STATUS "  - server  : Servidor do mensageiro")
message(STATUS "  - client  : Cliente do mensageiro")
message(STATUS "  - all     : Compila ambos (padrão)")
message(STATUS "========================================")
message(STATUS "")