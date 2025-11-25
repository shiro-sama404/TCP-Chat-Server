#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

/**
 * Classe Client
 * --------------
 * Responsável pela comunicação com o servidor via TCP.
 * Suporta envio/recepção de mensagens JSON com thread assíncrona.
 */
class Client
{
public:
    Client();
    ~Client();

    /**
     * Conecta ao servidor no host e porta especificados
     * @param host Endereço IP ou hostname do servidor
     * @param port Porta para conexão
     * @return true se a conexão for bem-sucedida, false caso contrário
     */
    bool connectToServer(const std::string& host, int port);

    /**
     * Envia uma string JSON para o servidor
     * @param json String contendo a mensagem JSON (será adicionado delimitador se necessário)
     * @return true se o envio ocorrer sem erros, false caso contrário
     */
    bool sendJson(const std::string& json);

    /**
     * Recebe uma mensagem JSON do servidor (operação não-bloqueante).
     * Utiliza buffer interno para reconstruir a mensagem completa.
     * @return std::optional contendo a mensagem ou nullopt se não houver dados completos
     */
    std::optional<std::string> receiveJson();

    /**
     * Fecha a conexão TCP e reseta o estado do socket
     */
    void disconnect();

    /**
     * Inicia a thread receptora para escutar mensagens do servidor em background
     */
    void startReceiverThread();

    /**
     * Sinaliza parada para a thread receptora e aguarda seu término (join)
     */
    void stopReceiverThread();

    /**
     * Obtém a próxima mensagem da fila interna thread-safe
     * @return std::optional com a mensagem ou nullopt se a fila estiver vazia
     */
    std::optional<std::string> popReceivedMessage();

    /**
     * Verifica o estado atual da conexão
     * @return true se o cliente estiver conectado, false caso contrário
     */
    bool isConnected() const { return connected; }

private:
    int sockfd;
    std::atomic<bool> connected;
    std::atomic<bool> receiving;
    std::thread receiverThread;

    /**
     * Buffer interno para acumular dados parciais em receiveJson()
     */
    std::string receiveBuffer;

    /**
     * Mutex para proteção de acesso concorrente à fila de mensagens
     */
    std::mutex queueMutex;

    /**
     * Fila de mensagens recebidas aguardando processamento
     */
    std::queue<std::string> messageQueue;

    /**
     * Função executada pela thread receptora.
     * Mantém um loop de leitura do socket enquanto conectado.
     */
    void receiverLoop();
};