#pragma once

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>    
#include <cstring>
#include <csignal>    
#include <random>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <pthread.h> 
#include <iomanip>

#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#include <functional>
#include <boost/asio.hpp>
#include <sys/wait.h>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <fstream>
#include <cstdio>

// MQTT
#include "mqtt_client.hpp"
#include "mqtt_publisher.hpp"
#include "mqtt_subscriber.hpp"
#include "mqtt_manager.hpp"
#include "mqtt_config.hpp"
