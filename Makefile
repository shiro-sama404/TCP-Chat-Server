# ==================== CONFIGURAÇÕES ====================
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -pthread -O2
INCLUDES = -Ilibs -Icommon -Iserver -Iclient
LDFLAGS = -pthread

# Diretórios
BUILD_DIR = build
COMMON_DIR = common
SERVER_DIR = server
CLIENT_DIR = client

# ==================== FONTES ====================
COMMON_SRC = $(COMMON_DIR)/protocol.cpp \
             $(COMMON_DIR)/socket_utils.cpp

SERVER_SRC = $(SERVER_DIR)/main.cpp \
             $(SERVER_DIR)/server.cpp \
             $(SERVER_DIR)/command_handler.cpp

CLIENT_SRC = $(CLIENT_DIR)/main.cpp \
             $(CLIENT_DIR)/client.cpp \
             $(CLIENT_DIR)/interface.cpp

# ==================== OBJETOS ====================
COMMON_OBJ = $(COMMON_SRC:.cpp=.o)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)

# ==================== ALVOS PRINCIPAIS ====================
.PHONY: all clean help

all: $(BUILD_DIR)/server $(BUILD_DIR)/client
	@echo ""
	@echo "✓ Compilação concluída com sucesso!"
	@echo "  - Servidor: $(BUILD_DIR)/server"
	@echo "  - Cliente:  $(BUILD_DIR)/client"
	@echo ""

$(BUILD_DIR)/server: $(COMMON_OBJ) $(SERVER_OBJ)
	@mkdir -p $(BUILD_DIR)
	@echo "[LINK] Criando executável do servidor..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/client: $(COMMON_OBJ) $(CLIENT_OBJ)
	@mkdir -p $(BUILD_DIR)
	@echo "[LINK] Criando executável do cliente..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# ==================== COMPILAÇÃO DE OBJETOS ====================
%.o: %.cpp
	@echo "[CXX]  $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ==================== LIMPEZA ====================
clean:
	@echo "Limpando arquivos de compilação..."
	@rm -f $(COMMON_DIR)/*.o $(SERVER_DIR)/*.o $(CLIENT_DIR)/*.o
	@rm -rf $(BUILD_DIR)
	@echo "✓ Limpeza concluída"

# ==================== AJUDA ====================
help:
	@echo "Makefile do TCP Chat Server"
	@echo ""
	@echo "Alvos disponíveis:"
	@echo "  make          - Compila servidor e cliente"
	@echo "  make clean    - Remove arquivos de compilação"
	@echo "  make help     - Mostra esta mensagem"
	@echo ""
	@echo "Executando:"
	@echo "  $(BUILD_DIR)/server [porta]"
	@echo "  $(BUILD_DIR)/client [host] [porta]"

# ==================== DEPENDÊNCIAS ====================
# Força recompilação se headers mudarem
$(COMMON_DIR)/protocol.o: $(COMMON_DIR)/protocol.hpp
$(COMMON_DIR)/socket_utils.o: $(COMMON_DIR)/socket_utils.hpp
$(SERVER_DIR)/server.o: $(SERVER_DIR)/server.hpp $(COMMON_DIR)/socket_utils.hpp
$(SERVER_DIR)/command_handler.o: $(SERVER_DIR)/command_handler.hpp $(SERVER_DIR)/server.hpp $(COMMON_DIR)/protocol.hpp
$(CLIENT_DIR)/client.o: $(CLIENT_DIR)/client.hpp $(COMMON_DIR)/socket_utils.hpp
$(CLIENT_DIR)/interface.o: $(CLIENT_DIR)/interface.hpp $(CLIENT_DIR)/client.hpp $(COMMON_DIR)/protocol.hpp