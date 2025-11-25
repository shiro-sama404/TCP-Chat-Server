#pragma once

#include <string>
#include <optional>

/**
 * Módulo SocketUtils
 * ------------------
 * Funções auxiliares para comunicação TCP com framing linha-por-linha.
 * Abstrai send/recv de baixo nível com tratamento de erros.
 */

namespace SocketUtils
{

/**
 * Envia uma mensagem JSON com framing (adiciona \n no final).
 * Retorna true se sucesso, false caso contrário.
 */
bool sendMessage(int sockfd, const std::string& json_message);

/**
 * Recebe uma mensagem JSON completa (até encontrar \n).
 * Em modo bloqueante: bloqueia até receber a linha completa.
 * Em modo não-bloqueante: retorna nullopt se não há dados disponíveis.
 * 
 * @param sockfd Socket file descriptor
 * @param buffer Buffer interno para acumular bytes (deve ser persistente entre chamadas)
 * @return Mensagem JSON (sem o \n), ou nullopt se não disponível/erro
 */
std::optional<std::string> receiveMessage(int sockfd, std::string& buffer);

/**
 * Define um socket como não-bloqueante.
 */
bool setNonBlocking(int sockfd);

/**
 * Define um socket como bloqueante.
 */
bool setBlocking(int sockfd);

/**
 * Fecha um socket de forma segura.
 */
void closeSocket(int& sockfd);

/**
 * Verifica se um socket está conectado.
 */
bool isSocketValid(int sockfd);

} // namespace SocketUtils