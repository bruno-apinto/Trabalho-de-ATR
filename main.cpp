#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <stdlib.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <semaphore>
#include <boost/asio.hpp>
#include <random>

#define ELEMENTOS_BUFFERS 10

//sensores e atuadores disponíveis no caminhão:
bool i_encoder; //Variável que simula a entrada de um encoder, que troca de estado a cada metro percorrido pelo robô
int i_lidar; //Resposta do sensor LIDAR do veículo exibindo a distância no eixo y, com relação à altura do robô 
bool o_liga_camera; //Comando para ligar a câmera e realizar fotos da falha detectada na superfície
int o_aceleracao; //Determina a aceleração do veículo em percentual (-100 a 100%)


//estados e comandos:
bool e_inspecao; //Estado que indica falha detectada na superfície e o robô está tirando fotos e navegando com velocidade limitada (1:falha, 0: sem falha)
bool e_automatico; //Estado que identifica o modo de operação do robô (0: manual, 1: automático)
bool c_automatico; //Comando para passar o robô para o modo automático (true). O reset desse comando
bool c_man; //Comando para passar o robô para o modo manual (true).
int j_sp_velocidade; //Setpoint de velocidade do robô para o controlador de velocidade.


//variaveis de condição buffers
std::condition_variable leitura_buffer_navegacao;
int dados_fila = 0;
std::condition_variable escrita_buffer_navegacao;

/**
 * @brief Tarefa que recebe comandos do sistema de operação remoto e traduz os comandos em setpoint de velocidade
 *  e liga/desliga para o controle de navegação. O comando de navegação que deve implementar a lógica de manual/ 
 * automático. Exerce a função de ESCRITOR sobre o BUFFER_NAVEGACAO
 * 
 */
void comando_navegacao(){

}

/**
 * @brief implementação de um controlador PID responsável pelo acionamento dos motores (controle de velocidade)
 *  a partir de um setpoint de velocidade recebido pelo Comando de Navegação. Exerce a função de LEITOR do BUFFER _NAVEGACAO
 * 
 */
void controle_navegacao(std::mutex &mtx, std::vector <float> &BUFFER){

    int idx = 0;

    for (int i = 0; i < 13; i++){
        
        idx++;
        idx = idx%ELEMENTOS_BUFFERS;
        
        std::unique_lock<std::mutex> lock (mtx);

        while(dados_fila == 0){
            leitura_buffer_navegacao.wait(lock);
        }

        std::cout << "Posição lida: " << BUFFER[idx] << std::endl;
        lock.unlock();
        dados_fila--;
        escrita_buffer_navegacao.notify_one();

    }

}

void distancia_percorrida(std::mutex &mtx, std::vector <float> &BUFFER){
    int idx = 0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(1.0f, 100.0f);

    for (int i = 0; i < 13; i++){
        idx++;
        idx = idx % ELEMENTOS_BUFFERS;
        float random_num = dis(gen);
    
        std::unique_lock<std::mutex> lock (mtx);
        
        while(dados_fila >=10){
            escrita_buffer_navegacao.wait(lock);
        }
        BUFFER[idx] = random_num;
        lock.unlock();

        dados_fila++;
        leitura_buffer_navegacao.notify_one();

        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Ajustado tempo para testes rápidos
    }
}

void reconstrucao_teto(std::mutex &mtx, std::vector <float> &BUFFER){

    std::unique_lock<std::mutex> lock (mtx);
    lock.unlock();
}

void inspecao_camera(){

}

void coletor_dados(std::mutex &mtx, std::vector <float> &BUFFER){

    std::unique_lock<std::mutex> lock (mtx);

    lock.unlock();
}

void operacao_remota(std::mutex &mtx, std::vector <float> &BUFFER){
    std::unique_lock<std::mutex> lock(mtx); // Protocolo de entrada
    lock.unlock(); // Protocolo de saída
}

void simulacao(){

}


int main (){

    //INICIALIZAÇÃO PROCESSOS

    pid_t pid;

    pid = fork();

   if (pid == 0){
        comando_navegacao ();
   }

   else if (pid < 0){
        perror ("Erro ao criar processo\n");
        exit(1);
   }

   else {

        //INICIALIZAÇÃO DE BUFFERS
        std::vector <float> BUFFER_NAVEGACAO (ELEMENTOS_BUFFERS); //posição do carrinho
        std::vector <float> BUFFER_NIVEL (ELEMENTOS_BUFFERS); //leitura de nivel

        //INICIALIZAÇÃO AS THREADS

        std::mutex mutex_navegacao;
        std::mutex mutex_nivel;

        std::vector <std::thread> threads_navegacao;
        threads_navegacao.emplace_back(distancia_percorrida, std::ref (mutex_navegacao), std::ref(BUFFER_NAVEGACAO));
        threads_navegacao.emplace_back(inspecao_camera);
        threads_navegacao.emplace_back(coletor_dados, std::ref (mutex_nivel), std::ref(BUFFER_NIVEL));
        threads_navegacao.emplace_back(reconstrucao_teto, std::ref (mutex_nivel), std::ref(BUFFER_NIVEL));

        controle_navegacao(std::ref (mutex_navegacao), std::ref(BUFFER_NAVEGACAO));

        for (int i = 0; i < threads_navegacao.size(); i++){
            threads_navegacao[i].detach();
        }
        


        std::cout << "Buffer navegação: ";

        for (auto i : BUFFER_NAVEGACAO){
            std::cout << i << " ";
        }

        std::cout << std::endl;
   }

    return 0;
}