#!/usr/bin/env python3

#Teste de Comunicação MQTT
#Script para testar a comunicação entre o robô e a operação remota


import sys
import time
from robot_mqtt_client import RobotMQTTClient

def print_header(text):
    print("\n" + "="*60)
    print(f"  {text}")
    print("="*60)

def test_mqtt_connection():
    """Teste 1: Conexão básica"""
    print_header("TESTE 1: Conexão MQTT")
    
    robot = RobotMQTTClient(broker_address="localhost")
    if robot.connect():
        print("✓ Conectado com sucesso")
        time.sleep(1)
        robot.disconnect()
        print("✓ Desconectado com sucesso")
        return True
    else:
        print("✗ Falha na conexão")
        return False

def test_publisher():
    """Teste 2: Publicação de dados"""
    print_header("TESTE 2: Publicação de Dados")
    
    robot = RobotMQTTClient(broker_address="localhost")
    robot.connect()
    
    # Registrar callback para atualizações
    updates = []
    def callback(topic, payload):
        updates.append((topic, payload))
    
    robot.register_callback(callback)
    
    print("Aguardando dados do robô...")
    time.sleep(3)
    
    if updates:
        print(f"✓ Recebidas {len(updates)} mensagens do robô:")
        for topic, payload in updates[-5:]:  # Mostrar últimas 5
            print(f"  {topic}: {payload}")
    else:
        print("✗ Nenhuma mensagem recebida (robô pode estar offline)")
    
    robot.disconnect()

def test_commands():
    """Teste 3: Envio de comandos"""
    print_header("TESTE 3: Envio de Comandos")
    
    robot = RobotMQTTClient(broker_address="localhost")
    robot.connect()
    
    print("Enviando comandos...")
    
    print("\n[1] Alterando para modo AUTOMÁTICO")
    robot.send_mode_command(automatic=True)
    time.sleep(0.5)
    
    print("[2] Definindo velocidade para 50%")
    robot.send_velocity_command(50)
    time.sleep(0.5)
    
    print("[3] Enviando comando: FORWARD")
    robot.send_navigation_command("forward")
    time.sleep(2)
    
    print("[4] Ligando câmera")
    robot.send_camera_command(enable=True)
    time.sleep(1)
    
    print("[5] Parando robô")
    robot.send_navigation_command("stop")
    robot.send_velocity_command(0)
    
    print("\n✓ Todos os comandos enviados")
    
    robot.disconnect()

def test_data_read():
    """Teste 4: Leitura de dados"""
    print_header("TESTE 4: Leitura de Dados do Robô")
    
    robot = RobotMQTTClient(broker_address="localhost")
    robot.connect()
    
    print("Aguardando dados do robô...")
    time.sleep(2)
    
    data = robot.get_robot_data()
    print("\nDados do robô:")
    for key, value in data.items():
        print(f"  {key}: {value}")
    
    robot.disconnect()

def test_stress():
    """Teste 5: Teste de carga"""
    print_header("TESTE 5: Teste de Carga (100 mensagens)")
    
    robot = RobotMQTTClient(broker_address="localhost")
    robot.connect()
    
    print("Enviando 100 comandos de velocidade...")
    start_time = time.time()
    
    for i in range(100):
        velocity = (i % 20) * 5 - 50  # Valores entre -50 e 45
        robot.send_velocity_command(velocity)
        if (i + 1) % 20 == 0:
            print(f"  {i + 1}/100 mensagens enviadas")
    
    elapsed = time.time() - start_time
    print(f"\n✓ 100 mensagens enviadas em {elapsed:.2f}s")
    print(f"  Taxa: {100/elapsed:.1f} mensagens/segundo")
    
    robot.disconnect()

def run_all_tests():
    """Executar todos os testes"""
    print("\n" + "="*60)
    print("  TESTES DE COMUNICAÇÃO MQTT - ROBÔ DE INSPEÇÃO")
    print("="*60)
    
    tests = [
        ("Conexão MQTT", test_mqtt_connection),
        ("Publicação de Dados", test_publisher),
        ("Envio de Comandos", test_commands),
        ("Leitura de Dados", test_data_read),
        ("Teste de Carga", test_stress),
    ]
    
    results = []
    for name, test_func in tests:
        try:
            result = test_func()
            results.append((name, result if result is not None else True))
        except Exception as e:
            print(f"\n✗ Erro no teste: {e}")
            results.append((name, False))
    
    # Resumo
    print_header("RESUMO DOS TESTES")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASSOU" if result else "✗ FALHOU"
        print(f"{status:10} - {name}")
    
    print(f"\nTotal: {passed}/{total} testes passaram")
    
    return passed == total

if __name__ == "__main__":
    import os
    
    # Verificar se o broker está rodando
    print("Verificando broker MQTT...")
    result = os.system("nc -zv localhost 1883 > /dev/null 2>&1")
    
    if result != 0:
        print("✗ Broker MQTT não está rodando em localhost:1883")
        print("\nPara iniciar o Mosquitto:")
        print("  macOS:  brew services start mosquitto")
        print("  Linux:  sudo systemctl start mosquitto")
        sys.exit(1)
    
    print("✓ Broker MQTT detectado\n")
    
    # Executar testes
    success = run_all_tests()
    
    sys.exit(0 if success else 1)
