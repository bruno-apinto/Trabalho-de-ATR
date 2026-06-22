#ifndef SINCRONIZACAO_H
#define SINCRONIZACAO_H

#include "includes.hpp"
#include "shared_memory.hpp"

#define ELEMENTOS_BUFFERS 10

#define PERIODO_COMANDO 500
#define PERIODO_CONTROLE 80
#define PERIODO_DISTANCIA 20
#define PERIODO_CAMERA 80
#define PERIODO_COLETOR 100
#define PERIODO_RECONSTRUCAO 80

struct VarCondSinc {
    // BUFFER 1: NAVEGAÇÃO
    std::vector<float> BUFFER_NAVEGACAO = std::vector<float>(ELEMENTOS_BUFFERS);
    int ESC_IDX_NAVEG = 0;     // Onde o produtor escreve
    int LER_IDX_NAVEG_C = 0;   // Onde o Controle lê
    int LER_IDX_NAVEG_R = 0;   // Onde a Reconstrução lê
    
    int disp_naveg_c = 0;      // Quantidade de dados pendentes para o Controle
    int disp_naveg_r = 0;      // Quantidade de dados pendentes para a Reconstrução
    std::mutex mtx_navegacao;  // Proteção exclusiva deste buffer

    // === BUFFER 2: NÍVEL ===
    std::vector<float> BUFFER_NIVEL = std::vector<float>(ELEMENTOS_BUFFERS);
    int ESC_IDX_NIVEL = 0;     // Onde a Reconstrução escreve
    int LER_IDX_NIVEL = 0;     // Onde o Coletor lê
    
    int disp_nivel = 0;        // Quantidade de dados pendentes para o Coletor
    std::mutex mtx_nivel;      // Proteção exclusiva deste buffer

    // === CÂMERA ===
    std::mutex mutex_camera;
};

typedef boost::asio::steady_timer tempo_tarefa;
typedef boost::asio::chrono::milliseconds milisegundos;

void log_message (const std::string& thread, const std::string& mensagem);
float numero_aleatorio_debugg();

void handler_signal (const boost::system::error_code& error, int signal_number, 
                    boost::asio::steady_timer* t, boost::asio::io_context::strand* strand_camera, 
                    std::mutex& mtx, MemoriaCompartilhada* shm, boost::asio::signal_set* sinais);

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
                        MemoriaCompartilhada* shm, VarCondSinc &sinc);
/**
 * @brief Registra a distância percorrida fornecida pelo encoder. Possui a função de ESCRITOR sobre o BUFFER_NAVEGACAO.
 * 
 * @param mtx mutex utilizado na variavel compartilhada
 * @param BUFFER historico de posições
 */
void distancia_percorrida(const boost::system::error_code& e,
                          boost::asio::steady_timer* t, 
                          boost::asio::io_context::strand* strand, 
                          MemoriaCompartilhada* shm, VarCondSinc &sinc);

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
                       MemoriaCompartilhada* shm, VarCondSinc &sinc);

/**
 * @brief Registra os dados coletados pelo lidar num Banco de Dados. Atua como LEITOR do BUFFER_NIVEL.
 * 
 * @param mtx mutex para sincronizar o buffer compartilhado
 * @param BUFFER buffer de dados coletados pelo lidar
 */
void coletor_dados(const boost::system::error_code& e,
                   boost::asio::steady_timer* t, 
                   boost::asio::io_context::strand* strand,
                   MemoriaCompartilhada* shm, VarCondSinc &sinc);

void inspecao_camera(const boost::system::error_code& e,
                     boost::asio::steady_timer* t, 
                     boost::asio::io_context::strand* strand,
                     std::mutex& mtx, MemoriaCompartilhada* shm);

void operacao_remota(std::mutex &mtx, std::vector <float> &BUFFER);

#endif