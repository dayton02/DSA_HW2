CXX = g++
CXXFLAGS = -std=c++17 -pedantic -Wall -Wextra -g

# TEST PASSED
# INPUT = test_cases/input_rectangle_with_two_holes.csv
# MODEL = test_cases/output_rectangle_with_two_holes.txt
# PLACEHOLDER = 7

# TEST PASSED
#INPUT = test_cases/input_cushion_with_hexagonal_hole.csv
#MODEL = test_cases/output_cushion_with_hexagonal_hole.txt
#PLACEHOLDER = 13

# TEST PASSED
# INPUT = test_cases/input_blob_with_two_holes.csv
# MODEL = test_cases/output_blob_with_two_holes.txt
# PLACEHOLDER = 17

# TEST PASSED
# INPUT = test_cases/input_wavy_with_three_holes.csv
# MODEL = test_cases/output_wavy_with_three_holes.txt
# PLACEHOLDER = 21

# TEST PASSED
# INPUT = test_cases/input_lake_with_two_islands.csv
# MODEL = test_cases/output_lake_with_two_islands.txt
# PLACEHOLDER = 17

# TESTS PASSED: 1,2,3,4,5,6,7,8,,10
INPUT = test_cases/input_original_09.csv
MODEL = test_cases/output_original_09.txt
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