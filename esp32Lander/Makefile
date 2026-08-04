CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
SRC = ship.cpp terrain.cpp game.cpp renderer_canvas.cpp renderer_pc.cpp

all: test_pc main_pc

test_pc: test_pc.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o test_pc test_pc.cpp $(SRC)

main_pc: main_pc.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o main_pc main_pc.cpp $(SRC)

clean:
	rm -f test_pc main_pc
	rm -rf frames

.PHONY: all clean
