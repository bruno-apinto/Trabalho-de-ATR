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


void comando_navegacao(){

}

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