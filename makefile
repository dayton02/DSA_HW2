CXX = g++
CXXFLAGS = -std=c++17 -pedantic -Wall -Wextra -g

#INPUT = test_cases/input_rectangle_with_two_holes.csv

INPUT = test_cases/input_original_02.csv
MODEL = test_cases/output_original_02.txt
PLACEHOLDER = 99
TARGET = main
OBJS = main.o apsc.o

all: $(TARGET)

$(TARGET):$(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp apsc.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

apsc.o: apsc.cpp apsc.hpp
	$(CXX) $(CXXFLAGS) -c apsc.cpp

run: $(TARGET)
	./$(TARGET) $(INPUT) $(PLACEHOLDER) $(MODEL)

clean:
	rm -f $(OBJS) $(TARGET)