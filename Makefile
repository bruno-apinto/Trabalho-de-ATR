TARGET = programa

CXX = g++

CXXFLAGS = -std=c++17 -I. -Iinclude -I/opt/homebrew/include -pthread

SRCS = src/sincronizacao.cpp src/simulacao.cpp main.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all