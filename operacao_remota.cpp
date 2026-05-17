#include "includes.hpp"

void operacao_remota(std::mutex& mtx, std::vector<float>& BUFFER){
    std::unique_lock<std::mutex> lock(mtx); // Protocolo de entrada
    lock.unlock(); // Protocolo de saída
}