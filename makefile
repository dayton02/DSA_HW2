CXX = g++
CXXFLAGS = -std=c++17 -pedantic -Wall -Wextra -g

INPUT = test_cases/input_original_01.csv
#INPUT = test_cases/input_rectangle_with_two_holes.csv
MODEL = test_cases/output_rectangle_with_two_holes.csv
PLACEHOLDER = 1000
TARGET = main
OBJS = main.o apsc.o

all: $(TARGET)

$(TARGET):$(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp apsc.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

aspc.o: apsc.cpp apsc.hpp
	$(CXX) $(CXXFLAGS) -c apsc.cpp

run: $(TARGET)
	./$(TARGET) $(INPUT) $(PLACEHOLDER) $(MODEL)

clean:
	rm -f $(OBJS) $(TARGET)