#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <stdlib.h>
#include <chrono>
#include <pthread.h>
#include <condition_variable>
#include <mutex>
#include <semaphore>
#include <boost/asio.hpp>

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

/* comandos sem definicao dada
c_direita -
Comando para acelerar o robô para a
direita.

c_esquerda -
Comando para acelerar o robô para a
esquerda.
c_para - Comando para parar o robô.
*/

//variáveis de sincronização
/* 
std::condition_variable devagar;
std::condition_variable camera; 
*/
std::mutex m;

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
void controle_navegacao(){

}

void distancia_percorrida(){

}

void reconstrucao_teto(){

    /* USAR SIGNAL AO INVES DE SEMAFORO
    devagar.notify_all(); //avisa o robô que uma falha foi detectada
    camera.notify_all(); //avisa a câmera que a falha foi detectada
    */
}

void inspecao_camera(){

    std::unique_lock<std::mutex> lock(m); // Protocolo de entrada
    /* USAR SIGNAL AO INVES DE SEMAFORO
    while(!(e_inspecao)){
        camera.wait(lock); // Espera até que uma falha seja detectada
    }
    */
    lock.unlock(); // Protocolo de saída
}

void coletor_dados(){

}

void operacao_remota(){
    std::unique_lock<std::mutex> lock(m); // Protocolo de entrada
    /* USAR SIGNAL AO INVES DE SEMAFORO
    while(!(e_inspecao)){
        devagar.wait(lock); // Espera até que uma falha seja detectada
    }
    */
    lock.unlock(); // Protocolo de saída
}

void simulacao(){

}


int main (){

    //INICIALIZAÇÃO PROCESSOS

    pid_t pid;

    pid = fork();

   if (pid == 0){
        controle_navegacao ();
   }

   else if (pid < 0){
        perror ("Erro ao criar processo\n");
        exit(1);
   }

   else {

        comando_navegacao ();

         //INICIALIZAÇÃO DE BUFFERS
        std::vector <float> BUFFER_NAVEGACAO (ELEMENTOS_BUFFERS); //posição do carrinho
        std::vector <float> BUFFER_NIVEL (ELEMENTOS_BUFFERS); //leitura de nivel

        //INICIALIZAÇÃO AS THREADS

        std::vector <std::thread> threads_navegacao;
        threads_navegacao.emplace_back(distancia_percorrida);
        threads_navegacao.emplace_back(inspecao_camera);
        threads_navegacao.emplace_back(coletor_dados);
        threads_navegacao.emplace_back(reconstrucao_teto);

   }

    return 0;
}