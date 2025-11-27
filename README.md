# TCP Chat Server - Mensageiro Cliente/Servidor

Implementação de um mensageiro rudimentar "um-a-um" para a disciplina de Redes de Computadores 2025/2, utilizando sockets TCP de baixo nível em C++17.

## 📋 Características

- Protocolo TCP com framing linha-por-linha (`\n`)
- Formato JSON UTF-8
- Multi-threaded (1 acceptor + 1 thread por cliente)
- Store-and-forward (filas de mensagens offline)
- Thread receptora assíncrona no cliente
- Tratamento robusto de erros e desconexões
- Apenas chamadas de socket de baixo nível (POSIX)

## 🛠️ Compilação

### Requisitos
- Sistema operacional: **Linux**
- Compilador: **g++** com suporte a C++17
- Bibliotecas: nlohmann/json (incluída em `libs/`)

### Método 1: Makefile (Recomendado)

```bash
make clean
make
```

### Método 2: CMake (Alternativo)

```bash
mkdir -p build && cd build
cmake ..
make
cd ..
```

**Ambos os métodos geram dois executáveis:**
- `build/server` (Makefile) ou `build/bin/server` (CMake)
- `build/client` (Makefile) ou `build/bin/client` (CMake)

## 🚀 Executando

### 1. Iniciar o Servidor

```bash
./build/server [porta]
```

Exemplo:
```bash
./build/server 12345
```

Se a porta não for especificada, o padrão é **12345**.

### 2. Conectar Clientes

Abra um ou mais terminais e execute:

```bash
./build/client [host] [porta]
```

Exemplos:
```bash
./build/client                   # Conecta em 127.0.0.1:12345 por padrão
./build/client 127.0.0.1 12345   # Especifica host e porta
```

## 📖 Comandos do Cliente

Uma vez conectado, você pode usar os seguintes comandos:

| Comando | Descrição | Exemplo |
|---------|-----------|---------|
| `register <apelido> "<Nome>"` | Registra um novo usuário | `register maria "Maria Silva"` |
| `login <apelido>` | Faz login com o apelido | `login maria` |
| `list` | Lista todos os usuários e status | `list` |
| `msg <dest> <texto>` | Envia mensagem privada | `msg joao Oi, tudo bem?` |
| `logout` | Faz logout da sessão | `logout` |
| `delete <apelido>` | Remove conta (deve estar deslogado) | `delete maria` |
| `quit` | Sai do programa | `quit` |
| `help` | Mostra ajuda | `help` |

## 📡 Protocolo

### Transporte
- **Camada**: TCP
- **Porta padrão**: 12345 (configurável)

### Framing
- **Método**: Linha por mensagem terminada em `\n`
- **Limite**: 16 KB por mensagem

### Formato
- **Codificação**: JSON UTF-8
- **Estrutura**: `{"type": "...", "payload": {...}}`

### Exemplos de Mensagens

**Registro:**
```json
{"type":"REGISTER","payload":{"nickname":"maria","fullname":"Maria Silva"}}
```

**Login:**
```json
{"type":"LOGIN","payload":{"nickname":"maria"}}
```

**Envio de mensagem:**
```json
{"type":"SEND_MSG","payload":{"to":"joao","text":"Olá!"}}
```

**Resposta de sucesso:**
```json
{"type":"OK"}
```

**Resposta de erro:**
```json
{"type":"ERROR","payload":{"message":"NICK_TAKEN"}}
```

## 🏗️ Arquitetura

### Servidor
- **Thread principal (acceptor)**: Bloqueia em `accept()` aguardando conexões
- **Threads worker**: Uma thread por cliente conectado
- **Sincronização**: `std::mutex` protegendo estruturas compartilhadas
- **Estruturas de dados**:
  - `users`: Mapa de usuários cadastrados
  - `sessions`: Mapa de sessões ativas (apelido → socket)
  - `messageQueues`: Filas de mensagens pendentes (store-and-forward)
  - `fdToNickname`: Mapeamento reverso (socket → apelido)

### Cliente
- **Thread principal**: Interface CLI e envio de comandos
- **Thread receptora**: Recebe mensagens do servidor assincronamente
- **Fila thread-safe**: Armazena mensagens recebidas para exibição

## 📁 Estrutura do Projeto

```
TCP-Chat-Server/
├── Makefile                    # Compilação
├── CMakeLists.txt              # Compilação
├── README.md                   # Este arquivo
├── LICENSE                     # Licença deste projeto
├── docs/
│   ├── relatório.pdf           # Relatório deste trabalho
├── common/                     # Código compartilhado
│   ├── protocol.hpp/cpp        # Validação e builders JSON
│   └── socket_utils.hpp/cpp    # Funções auxiliares de socket
├── server/
│   ├── main.cpp                # Entry point do servidor
│   ├── server.hpp/cpp          # Classe Server
│   └── command_handler.hpp/cpp # Processamento de comandos
├── client/
│   ├── main.cpp                # Entry point do cliente
│   ├── client.hpp/cpp          # Classe Client
│   └── interface.hpp/cpp       # Interface CLI
├── tests/
│   ├── test_suite.sh           # Arquivo automatizado de testes
└── libs/
    └── nlohmann/json.hpp       # Biblioteca JSON
```

## ⚙️ Limitações e Configurações

| Item | Valor |
|------|-------|
| Porta padrão | 12345 |
| Tamanho máximo de apelido | 32 caracteres |
| Tamanho máximo de nome completo | 128 caracteres |
| Tamanho máximo de mensagem | 4096 bytes |
| Tamanho máximo de JSON | 16 KB |
| Conexões simultâneas | Limitado pelo SO |

## 🐛 Tratamento de Erros

O sistema trata os seguintes cenários:

- ✅ Apelido duplicado no registro
- ✅ Login de usuário inexistente
- ✅ Segundo login do mesmo apelido
- ✅ Envio de mensagem sem estar logado
- ✅ Envio para usuário inexistente
- ✅ Desconexão abrupta do cliente
- ✅ Mensagens malformadas
- ✅ Queda do servidor

## 📚 Referências

- **Sockets POSIX**: `man 2 socket`, `man 2 bind`, `man 2 connect`, etc.
- **JSON para C++**: [nlohmann/json](https://github.com/nlohmann/json)
- **C++17 Threading**: `std::thread`, `std::mutex`, `std::atomic`

## 👥 Autores
| [<img loading="lazy" src="https://avatars.githubusercontent.com/u/68046889?v=4" width=115><br><sub>Arthur de Andrade</sub>](https://github.com/shiro-sama404) |  [<img loading="lazy" src="https://avatars.githubusercontent.com/u/91064992?v=4" width=115><br><sub>Fernanda Neves</sub>](https://github.com/Fernanda-Neves410) |  [<img loading="lazy" src="https://avatars.githubusercontent.com/u/144397400?v=4" width=115><br><sub>Jenniffer Checchia</sub>](https://github.com/Jenn-Checchia) |
| :---: | :---: | :---: |

## 📄 Licença

Este projeto foi desenvolvido para fins acadêmicos na disciplina de Redes de Computadores 2025/2 da UFMS sob licença MIT.
