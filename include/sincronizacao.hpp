#ifndef SINCRONIZACAO_H
#define SINCRONIZACAO_H

#include "includes.hpp"
#include "shared_memory.hpp"

#define ELEMENTOS_BUFFERS 10

typedef boost::asio::steady_timer tempo_tarefa;
typedef boost::asio::chrono::microseconds microssegundos;

const int SHM_SIZE = 1024; // Size of the shared memory segment

// Sincronização e Condições
std::condition_variable camera;
int eventos_camera = 0;

// Variaveis de condição buffers (Mantidas para controle de fluxo rápido)
std::condition_variable leitura_buffer_navegacao;
std::condition_variable escrita_buffer_navegacao;

int AR_NAVEGACAO = 0;
int WR_NAVEGACAO = 0;
int AW_NAVEGACAO = 0;
int WW_NAVEGACAO = 0;

std::condition_variable leitura_buffer_nivel; 
std::condition_variable escrita_buffer_nivel;
int dados_nivel = 0;

// Funções auxiliares de debbug
std::mutex mutex_log;

void log_message (const std::string& thread, const std::string& mensagem);
float numero_aleatorio_debugg();

/**
 * @brief Tarefa que recebe comandos do sistema de operação remoto e traduz os comandos em setpoint de velocidade
 *  e liga/desliga para o controle de navegação. O comando de navegação que deve implementar a lógica de manual/ 
 * automático. Exerce a função de ESCRITOR sobre o BUFFER_NAVEGACAO
 * 
 */
void comando_navegacao(const boost::system::error_code& /*e*/,
           boost::asio::steady_timer* t, 
           boost::asio::io_context::strand* strand,
            MemoriaCompartilhada* shm);

/**
 * @brief implementação de um controlador PID responsável pelo acionamento dos motores (controle de velocidade)
 *  a partir de um setpoint de velocidade recebido pelo Comando de Navegação. Exerce a função de LEITOR do BUFFER _NAVEGACAO
 * 
 * @param mtx mutex utilizado
 * @param BUFFER historico de posições
 */
void controle_navegacao(const boost::system::error_code& e,
                        boost::asio::steady_timer* t, 
                        boost::asio::io_context::strand* strand,
                        std::mutex &mtx, 
                        std::vector<float> &BUFFER,
                        MemoriaCompartilhada* shm);

/**
 * @brief Registra a distância percorrida fornecida pelo encoder. Possui a função de ESCRITOR sobre o BUFFER_NAVEGACAO.
 * 
 * @param mtx mutex utilizado na variavel compartilhada
 * @param BUFFER historico de posições
 */
void distancia_percorrida(const boost::system::error_code& e,
                          boost::asio::steady_timer* t, 
                          boost::asio::io_context::strand* strand, 
                          std::mutex &mtx, 
                          std::vector<float> &BUFFER,
                          MemoriaCompartilhada* shm);

/**
 * @brief Recebe os valores do lidar para reconstruir o teto do túnel. Atua como
 * ESCRITOR do BUFFER_NIVEL
 * 
 * @param mtx mutex para buffer compartilhado
 * @param BUFFER vetor de dados do nível da distancia do teto
 */
void reconstrucao_teto(const boost::system::error_code& e,
                       boost::asio::steady_timer* t, 
                       boost::asio::io_context::strand* strand,
                       std::mutex &mtx_navegacao, std::mutex &mtx_nivel, std::mutex &mtx_camera, 
                       std::vector<float> &BUFFER_NAVEGACAO, std::vector<float> &BUFFER_NIVEL, 
                       MemoriaCompartilhada* shm)

/**
 * @brief Registra os dados coletados pelo lidar num Banco de Dados. Atua como LEITOR do BUFFER_NIVEL.
 * 
 * @param mtx mutex para sincronizar o buffer compartilhado
 * @param BUFFER buffer de dados coletados pelo lidar
 */
void coletor_dados(const boost::system::error_code& e,
                   boost::asio::steady_timer* t, 
                   boost::asio::io_context::strand* strand,
                   std::mutex &mtx, std::vector<float> &BUFFER,
                   MemoriaCompartilhada* shm);

void inspecao_camera(const boost::system::error_code& e,
                     boost::asio::steady_timer* t, 
                     boost::asio::io_context::strand* strand,
                     std::mutex& mtx, MemoriaCompartilhada* shm);

void operacao_remota(std::mutex &mtx, std::vector <float> &BUFFER);


#endif