TARGET = programa

CXX = g++

CXXFLAGS = -std=c++17 -Wall -Wextra -I. -Iinclude -I/opt/homebrew/include -I/usr/local/include -I/opt/homebrew/opt/boost/include -pthread

LDFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lpaho-mqttpp3 -lpaho-mqtt3c -lpthread -Wl,-rpath,/opt/homebrew/lib -Wl,-rpath,/usr/local/lib

SRCS = main.cpp src/sincronizacao.cpp src/simulacao.cpp src/mqtt_client.cpp src/mqtt_publisher.cpp src/mqtt_subscriber.cpp src/mqtt_manager.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all