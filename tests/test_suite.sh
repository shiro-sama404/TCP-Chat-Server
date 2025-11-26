#!/bin/bash

# ==============================================================================
# Suite de Testes - TCP Chat Server
# ==============================================================================
# Este script automatiza os testes funcionais do mensageiro.
# Configurar: chmod +x
# Executar  : test_suite.sh && ./test_suite.sh
# ==============================================================================

set -e  # Para na primeira falha

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Contadores
TESTS_PASSED=0
TESTS_FAILED=0

# Funções auxiliares
print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

print_test() {
    echo -e "${YELLOW}[TESTE $1]${NC} $2"
}

print_success() {
    echo -e "${GREEN}✓ PASSOU${NC}: $1\n"
    TESTS_PASSED=$((TESTS_PASSED+1))
}

print_fail() {
    echo -e "${RED}✗ FALHOU${NC}: $1"
    echo -e "${RED}Motivo: $2${NC}\n"
    TESTS_FAILED=$((TESTS_FAILED+1))
}

print_info() {
    echo -e "${BLUE}→${NC} $1"
}

cleanup() {
    print_info "Limpando processos..."
    pkill -f "build/server" 2>/dev/null || true
    pkill -f "build/client" 2>/dev/null || true
    sleep 1
}

# ==============================================================================
# TESTE 1: Compilação
# ==============================================================================
test_compilation() {
    print_header "TESTE 1: COMPILAÇÃO"
    
    print_test "1.1" "Limpando build anterior"
    make clean &>/dev/null
    print_success "Build limpo"
    
    print_test "1.2" "Compilando com Makefile"
    if make &>/dev/null; then
        print_success "Compilação com Makefile"
    else
        print_fail "Compilação com Makefile" "make falhou"
        exit 1
    fi
    
    print_test "1.3" "Verificando executáveis"
    if [ -f "build/server" ] && [ -f "build/client" ]; then
        print_success "Executáveis criados (build/server e build/client)"
    else
        print_fail "Executáveis criados" "Arquivos não encontrados"
        exit 1
    fi
    
    print_test "1.4" "Verificando permissões de execução"
    if [ -x "build/server" ] && [ -x "build/client" ]; then
        print_success "Executáveis têm permissão de execução"
    else
        print_fail "Permissões de execução" "chmod +x necessário"
        exit 1
    fi
}

# ==============================================================================
# TESTE 2: Inicialização do Servidor
# ==============================================================================
test_server_startup() {
    print_header "TESTE 2: INICIALIZAÇÃO DO SERVIDOR"
    
    cleanup
    
    print_test "2.1" "Iniciando servidor na porta 12345"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    if ps -p $SERVER_PID > /dev/null; then
        print_success "Servidor iniciou (PID: $SERVER_PID)"
    else
        print_fail "Servidor iniciou" "Processo morreu"
        cat /tmp/server.log
        return 1
    fi
    
    print_test "2.2" "Verificando porta TCP 12345"
    if netstat -tln 2>/dev/null | grep -q ":12345" || ss -tln 2>/dev/null | grep -q ":12345"; then
        print_success "Porta 12345 em LISTEN"
    else
        print_fail "Porta 12345 em LISTEN" "Porta não aberta"
        kill $SERVER_PID 2>/dev/null
        return 1
    fi
    
    print_test "2.3" "Testando múltiplas instâncias (deve falhar)"
    if ./build/server 12345 &>/tmp/server2.log & SERVER2_PID=$!; then
        sleep 1
        if ps -p $SERVER2_PID > /dev/null; then
            print_fail "Rejeição de porta duplicada" "Segunda instância aceitou"
            kill $SERVER2_PID 2>/dev/null
        else
            print_success "Rejeição de porta duplicada (bind falhou corretamente)"
        fi
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 3: Conexão Cliente-Servidor
# ==============================================================================
test_client_connection() {
    print_header "TESTE 3: CONEXÃO CLIENTE-SERVIDOR"
    
    cleanup
    
    print_test "3.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "3.2" "Conectando cliente ao servidor"
    # Cliente deve conectar e fechar com 'quit'
    echo "quit" | timeout 5 ./build/client 127.0.0.1 12345 &>/tmp/client.log
    
    if grep -q "Conectado" /tmp/client.log || grep -q "conectado" /tmp/client.log; then
        print_success "Cliente conectou ao servidor"
    else
        print_fail "Cliente conectou ao servidor" "Mensagem de conexão não encontrada"
        cat /tmp/client.log
    fi
    
    print_test "3.3" "Verificando desconexão limpa"
    sleep 1
    if ! pgrep -f "build/client" > /dev/null; then
        print_success "Cliente desconectou corretamente"
    else
        print_fail "Cliente desconectou" "Processo ainda ativo"
        pkill -f "build/client"
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 4: Registro de Usuários
# ==============================================================================
test_user_registration() {
    print_header "TESTE 4: REGISTRO DE USUÁRIOS"
    
    cleanup
    
    print_test "4.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "4.2" "Registrando usuário 'maria'"
    {
        sleep 0.5
        echo 'register maria "Maria Silva"'
        sleep 0.5
        echo "quit"
    } | timeout 5 ./build/client 127.0.0.1 12345 &>/tmp/client_maria.log
    
    if grep -q "OK" /tmp/client_maria.log || grep -q "sucesso" /tmp/client_maria.log; then
        print_success "Registro de usuário 'maria'"
    else
        print_fail "Registro de usuário 'maria'" "Resposta OK não recebida"
        cat /tmp/client_maria.log
    fi
    
    print_test "4.3" "Tentando registrar 'maria' novamente (deve falhar)"
    {
        sleep 0.5
        echo 'register maria "Maria Duplicada"'
        sleep 0.5
        echo "quit"
    } | timeout 5 ./build/client 127.0.0.1 12345 &>/tmp/client_maria2.log
    
    if grep -q "NICK_TAKEN\|Erro" /tmp/client_maria2.log; then
        print_success "Rejeição de apelido duplicado"
    else
        print_fail "Rejeição de apelido duplicado" "Erro não detectado"
        cat /tmp/client_maria2.log
    fi
    
    print_test "4.4" "Registrando segundo usuário 'joao'"
    {
        sleep 0.5
        echo 'register joao "João Santos"'
        sleep 0.5
        echo "quit"
    } | timeout 5 ./build/client 127.0.0.1 12345 &>/tmp/client_joao.log
    
    if grep -q "OK\|sucesso" /tmp/client_joao.log; then
        print_success "Registro de usuário 'joao'"
    else
        print_fail "Registro de usuário 'joao'" "Resposta OK não recebida"
        cat /tmp/client_joao.log
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 5: Login e Logout
# ==============================================================================
test_login_logout() {
    print_header "TESTE 5: LOGIN E LOGOUT"
    
    cleanup
    
    print_test "5.1" "Iniciando servidor e registrando usuários"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    # Registra maria
    echo -e 'register maria "Maria Silva"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    sleep 0.5
    
    print_test "5.2" "Login do usuário 'maria'"
    {
        sleep 0.5
        echo "login maria"
        sleep 0.5
        echo "logout"
        sleep 0.5
        echo "quit"
    } | timeout 5 ./build/client 127.0.0.1 12345 &>/tmp/client_login.log
    
    if grep -q "OK\|sucesso" /tmp/client_login.log; then
        print_success "Login de 'maria'"
    else
        print_fail "Login de 'maria'" "Resposta OK não recebida"
        cat /tmp/client_login.log
    fi
    
    print_test "5.3" "Login de usuário inexistente (deve falhar)"
    {
        sleep 0.5
        echo "login inexistente"
        sleep 0.5
        echo "quit"
    } | timeout 5 ./build/client 127.0.0.1 12345 &>/tmp/client_nouser.log
    
    if grep -q "NO_SUCH_USER\|Erro" /tmp/client_nouser.log; then
        print_success "Rejeição de usuário inexistente"
    else
        print_fail "Rejeição de usuário inexistente" "Erro não detectado"
        cat /tmp/client_nouser.log
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 6: Envio de Mensagens (Online)
# ==============================================================================
test_messaging_online() {
    print_header "TESTE 6: ENVIO DE MENSAGENS (ONLINE)"
    
    cleanup
    
    print_test "6.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "6.2" "Registrando maria e joao"
    echo -e 'register maria "Maria Silva"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    echo -e 'register joao "João Santos"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    sleep 0.5
    
    print_test "6.3" "Maria e João fazem login"
    # João fica online esperando mensagem
    {
        sleep 0.5
        echo "login joao"
        sleep 3  # Aguarda mensagem de maria
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_joao.log &
    JOAO_PID=$!
    
    sleep 1
    
    # Maria envia mensagem
    {
        sleep 0.5
        echo "login maria"
        sleep 0.5
        echo "msg joao Oi João, mensagem de teste!"
        sleep 1
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_maria.log
    
    sleep 1
    
    print_test "6.4" "Verificando se Maria enviou com sucesso"
    if grep -q "OK\|sucesso" /tmp/client_maria.log; then
        print_success "Maria enviou mensagem"
    else
        print_fail "Maria enviou mensagem" "Resposta OK não recebida"
        cat /tmp/client_maria.log
    fi
    
    print_test "6.5" "Verificando se João recebeu a mensagem"
    wait $JOAO_PID 2>/dev/null || true
    
    if grep -q "maria.*mensagem de teste\|Oi João" /tmp/client_joao.log; then
        print_success "João recebeu mensagem de Maria"
    else
        print_fail "João recebeu mensagem" "Mensagem não encontrada no log de João"
        cat /tmp/client_joao.log
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 7: Store-and-Forward (Offline)
# ==============================================================================
test_store_and_forward() {
    print_header "TESTE 7: STORE-AND-FORWARD (OFFLINE)"
    
    cleanup
    
    print_test "7.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "7.2" "Registrando maria e joao"
    echo -e 'register maria "Maria Silva"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    echo -e 'register joao "João Santos"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    sleep 0.5
    
    print_test "7.3" "Maria envia mensagem para João (João offline)"
    {
        sleep 0.5
        echo "login maria"
        sleep 0.5
        echo "msg joao Mensagem para voce offline!"
        sleep 0.5
        echo "logout"
        sleep 0.5
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_maria_offline.log
    
    if grep -q "OK\|sucesso" /tmp/client_maria_offline.log; then
        print_success "Maria enviou mensagem (João offline)"
    else
        print_fail "Maria enviou mensagem" "Resposta OK não recebida"
        cat /tmp/client_maria_offline.log
    fi
    
    sleep 1
    
    print_test "7.4" "João faz login e recebe mensagens pendentes"
    {
        sleep 0.5
        echo "login joao"
        sleep 2  # Aguarda entrega de mensagens pendentes
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_joao_offline.log
    
    if grep -q "maria.*offline\|Mensagem para voce" /tmp/client_joao_offline.log; then
        print_success "João recebeu mensagem pendente (store-and-forward)"
    else
        print_fail "Store-and-forward" "Mensagem pendente não entregue"
        cat /tmp/client_joao_offline.log
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 8: Listagem de Usuários
# ==============================================================================
test_list_users() {
    print_header "TESTE 8: LISTAGEM DE USUÁRIOS"
    
    cleanup
    
    print_test "8.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "8.2" "Registrando múltiplos usuários"
    echo -e 'register maria "Maria Silva"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    echo -e 'register joao "João Santos"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    echo -e 'register carol "Carol Lima"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    sleep 0.5
    
    print_test "8.3" "Listando usuários (todos offline)"
    {
        sleep 0.5
        echo "list"
        sleep 1
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_list.log
    
    if grep -q "maria" /tmp/client_list.log && grep -q "joao" /tmp/client_list.log; then
        print_success "Listagem de usuários (offline)"
    else
        print_fail "Listagem de usuários" "Usuários não encontrados"
        cat /tmp/client_list.log
    fi
    
    print_test "8.4" "Maria faz login e lista usuários"
    {
        sleep 0.5
        echo "login maria"
        sleep 0.5
        echo "list"
        sleep 1
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_list_online.log &
    
    sleep 2
    
    # João também faz login
    {
        sleep 0.5
        echo "login joao"
        sleep 0.5
        echo "list"
        sleep 1
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_list_online2.log
    
    sleep 1
    
    if grep -q "ONLINE" /tmp/client_list_online2.log; then
        print_success "Listagem mostra status ONLINE/OFFLINE"
    else
        print_fail "Status ONLINE/OFFLINE" "Status não encontrado"
        cat /tmp/client_list_online2.log
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 9: Deleção de Usuários
# ==============================================================================
test_user_deletion() {
    print_header "TESTE 9: DELEÇÃO DE USUÁRIOS"
    
    cleanup
    
    print_test "9.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "9.2" "Registrando usuário 'temp'"
    echo -e 'register temp "Usuario Temporario"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    sleep 0.5
    
    print_test "9.3" "Tentando deletar enquanto logado (deve falhar)"
    {
        sleep 0.5
        echo "login temp"
        sleep 0.5
        echo "delete temp"
        sleep 0.5
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_delete_fail.log
    
    if grep -q "BAD_STATE\|Erro" /tmp/client_delete_fail.log; then
        print_success "Rejeição de deleção com usuário logado"
    else
        print_fail "Rejeição de deleção logado" "Erro não detectado"
        cat /tmp/client_delete_fail.log
    fi
    
    print_test "9.4" "Deletando após logout"
    {
        sleep 0.5
        echo "login temp"
        sleep 0.5
        echo "logout"
        sleep 0.5
        echo "delete temp"
        sleep 0.5
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_delete_ok.log
    
    if grep -q "OK\|sucesso" /tmp/client_delete_ok.log; then
        print_success "Deleção de usuário após logout"
    else
        print_fail "Deleção de usuário" "Resposta OK não recebida"
        cat /tmp/client_delete_ok.log
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 10: Reconexão após Queda
# ==============================================================================
test_reconnection() {
    print_header "TESTE 10: RECONEXÃO APÓS QUEDA"
    
    cleanup
    
    print_test "10.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "10.2" "Registrando maria"
    echo -e 'register maria "Maria Silva"\nquit' | timeout 5 ./build/client 127.0.0.1 12345 &>/dev/null
    sleep 0.5
    
    print_test "10.3" "Maria faz login"
    {
        sleep 0.5
        echo "login maria"
        sleep 2
        # Não faz logout, simula queda
    } | timeout 5 ./build/client 127.0.0.1 12345 &>/tmp/client_maria_crash.log &
    MARIA_PID=$!
    
    sleep 1
    
    print_test "10.4" "Matando cliente (simulando queda)"
    kill -9 $MARIA_PID 2>/dev/null || true
    sleep 2
    
    print_test "10.5" "Maria tenta login novamente (deve aceitar)"
    {
        sleep 0.5
        echo "login maria"
        sleep 1
        echo "quit"
    } | timeout 10 ./build/client 127.0.0.1 12345 &>/tmp/client_maria_reconnect.log
    
    if grep -q "OK\|sucesso" /tmp/client_maria_reconnect.log; then
        print_success "Reconexão após queda do cliente"
    else
        print_fail "Reconexão após queda" "Login rejeitado"
        cat /tmp/client_maria_reconnect.log
    fi
    
    cleanup
}

# ==============================================================================
# TESTE 11: Múltiplos Clientes Simultâneos
# ==============================================================================
test_multiple_clients() {
    print_header "TESTE 11: MÚLTIPLOS CLIENTES SIMULTÂNEOS"
    
    cleanup
    
    print_test "11.1" "Iniciando servidor"
    ./build/server 12345 &>/tmp/server.log &
    SERVER_PID=$!
    sleep 1
    
    print_test "11.2" "Conectando 5 clientes simultaneamente"
    
    for i in {1..5}; do
        {
            sleep 0.5
            echo "register user$i \"Usuario $i\""
            sleep 0.5
            echo "login user$i"
            sleep 1
            echo "list"
            sleep 1
            echo "quit"
        } | timeout 15 ./build/client 127.0.0.1 12345 &>/tmp/client_$i.log &
    done
    
    sleep 5
    
    print_test "11.3" "Verificando se todos os clientes conectaram"
    SUCCESS_COUNT=0
    for i in {1..5}; do
        if grep -q "OK\|sucesso" /tmp/client_$i.log; then
            SUCCESS_COUNT=$((SUCCESS_COUNT+1))
        fi
    done
    
    if [ $SUCCESS_COUNT -eq 5 ]; then
        print_success "5/5 clientes conectaram simultaneamente"
    else
        print_fail "Múltiplos clientes" "Apenas $SUCCESS_COUNT/5 conectaram"
    fi
    
    cleanup
}

# ==============================================================================
# EXECUÇÃO DOS TESTES
# ==============================================================================

main() {
    clear
    echo -e "${GREEN}"
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║        SUITE DE TESTES - TCP CHAT SERVER                   ║"
    echo "║        Redes de Computadores 2025/2                        ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo -e "${NC}\n"
    
    # Verifica se está no diretório correto
    if [ ! -f "Makefile" ]; then
        echo -e "${RED}ERRO: Execute este script na raiz do projeto (onde está o Makefile)${NC}"
        exit 1
    fi
    
    # Executa testes
    test_compilation
    test_server_startup
    test_client_connection
    test_user_registration
    test_login_logout
    test_messaging_online
    test_store_and_forward
    test_list_users
    test_user_deletion
    test_reconnection
    test_multiple_clients
    
    # Relatório final
    print_header "RELATÓRIO FINAL"
    TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED))
    
    echo -e "Total de testes: ${BLUE}$TOTAL_TESTS${NC}"
    echo -e "Testes passados: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Testes falhados: ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}╔════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║  ✓ TODOS OS TESTES PASSARAM!           ║${NC}"
        echo -e "${GREEN}║  Sistema pronto para demonstração      ║${NC}"
        echo -e "${GREEN}╚════════════════════════════════════════╝${NC}\n"
        exit 0
    else
        echo -e "\n${RED}╔════════════════════════════════════════╗${NC}"
        echo -e "${RED}║  ✗ ALGUNS TESTES FALHARAM              ║${NC}"
        echo -e "${RED}║  Revise os logs acima                  ║${NC}"
        echo -e "${RED}╚════════════════════════════════════════╝${NC}\n"
        exit 1
    fi
}

# Tratamento de Ctrl+C
trap cleanup EXIT INT TERM

# Executa
main