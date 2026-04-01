CXX = g++
CXXFLAGS = -std=c++17 -pedantic -Wall -Wextra

# Requirement: Build an executable called 'simplify'
TARGET = simplify
OBJS = main.o apsc.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp apsc.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

apsc.o: apsc.cpp apsc.hpp
	$(CXX) $(CXXFLAGS) -c apsc.cpp

# Updated run command to match the required executable name
run: $(TARGET)
	./$(TARGET) $(INPUT) $(PLACEHOLDER)

clean:
	rm -f $(OBJS) $(TARGET)