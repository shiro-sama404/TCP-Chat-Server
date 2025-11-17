#pragma once

#include <string>
#include <optional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

/**
 * Classe Client
 * --------------
 * Responsável pela comunicação com o servidor via TCP.
 * Permite enviar e receber mensagens no formato JSON (UTF-8)
 * seguindo o protocolo de framing “linha por mensagem (\n)”.
 */
class Client {
public:
    Client();
    ~Client();

    // Conecta ao servidor no host e porta especificados.
    bool connectToServer(const std::string& host, int port);

    // Envia uma string JSON (com \n no final).
    bool sendJson(const std::string& json);

    // Recebe uma mensagem JSON do servidor (bloqueante ou não).
    std::optional<std::string> receiveJson();

    // Fecha a conexão TCP.
    void disconnect();

    // Inicia uma thread para escutar mensagens do servidor (modo assíncrono).
    void startReceiverThread();

    // Para a thread receptora.
    void stopReceiverThread();

    // Obtém a próxima mensagem recebida (fila interna).
    std::optional<std::string> popReceivedMessage();

private:
    int sockfd;
    std::atomic<bool> connected;
    std::atomic<bool> receiving;
    std::thread receiverThread;

    std::mutex queueMutex;
    std::queue<std::string> messageQueue;

    // Função interna usada pela thread receptora.
    void receiverLoop();
};
