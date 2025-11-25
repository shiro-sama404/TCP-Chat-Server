#include "socket_utils.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <iostream>

namespace SocketUtils {

bool sendMessage(int sockfd, const std::string& json_message) {
    if (sockfd < 0) return false;
    
    std::string message = json_message + "\n";
    const char* buffer = message.c_str();
    size_t total_sent = 0;
    size_t len = message.size();

    while (total_sent < len) {
        ssize_t bytes = send(sockfd, buffer + total_sent, len - total_sent, 0);
        
        if (bytes < 0) {
            if (errno == EINTR) continue; // Interrompido, tentar novamente
            std::cerr << "[SocketUtils] Erro ao enviar: " << strerror(errno) << std::endl;
            return false;
        }
        
        total_sent += bytes;
    }
    
    return true;
}

std::optional<std::string> receiveMessage(int sockfd, std::string& buffer) {
    if (sockfd < 0) return std::nullopt;
    
    char ch;
    
    while (true) {
        ssize_t bytes = recv(sockfd, &ch, 1, 0);
        
        if (bytes < 0) {
            // Não-bloqueante: sem dados disponíveis
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return std::nullopt;
            }
            
            // Interrompido: continuar
            if (errno == EINTR) {
                continue;
            }
            
            // Erro real
            std::cerr << "[SocketUtils] Erro ao receber: " << strerror(errno) << std::endl;
            return std::nullopt;
        }
        
        if (bytes == 0) {
            // Conexão fechada pelo peer
            return std::nullopt;
        }
        
        // Encontrou fim de linha
        if (ch == '\n') {
            std::string message = buffer;
            buffer.clear();
            return message;
        }
        
        buffer += ch;
        
        // Proteção contra mensagens gigantescas
        if (buffer.size() > 16384) { // 16KB limite
            std::cerr << "[SocketUtils] Mensagem muito longa, descartando" << std::endl;
            buffer.clear();
            return std::nullopt;
        }
    }
}

bool setNonBlocking(int sockfd) {
    if (sockfd < 0) return false;
    
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        std::cerr << "[SocketUtils] Erro ao obter flags: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "[SocketUtils] Erro ao definir non-blocking: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool setBlocking(int sockfd) {
    if (sockfd < 0) return false;
    
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        std::cerr << "[SocketUtils] Erro ao obter flags: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        std::cerr << "[SocketUtils] Erro ao definir blocking: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

void closeSocket(int& sockfd) {
    if (sockfd >= 0) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
    }
}

bool isSocketValid(int sockfd) {
    return sockfd >= 0;
}

} // namespace SocketUtils