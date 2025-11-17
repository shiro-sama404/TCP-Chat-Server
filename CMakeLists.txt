cmake_minimum_required(VERSION 3.16)
project(TCPChatClient LANGUAGES CXX)

# --- Configurações Gerais ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Fontes por pasta ---
file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS "src/chat_server/*.cpp")
file(GLOB CLI_SOURCES    CONFIGURE_DEPENDS "src/cli/*.cpp")

set(SOURCES
    ${CLIENT_SOURCES}
    ${CLI_SOURCES}
    src/main.cpp
)

# --- Criação do Executável ---
add_executable(chat_client ${SOURCES})

# --- Diretórios de include ---
target_include_directories(chat_client PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/chat_server
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/libs
)

# --- Saída do executável ---
set_target_properties(chat_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "Compilando TCP Chat (cliente por enquanto)")
