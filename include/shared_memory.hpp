#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

struct MemoriaCompartilhada {

    //sensores e atuadores disponíveis no carrinho:
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
};

#endif