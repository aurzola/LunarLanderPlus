CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
SRC = ship.cpp terrain.cpp game.cpp renderer_canvas.cpp renderer_pc.cpp

all: test_pc main_pc demo_sim demo_render

test_pc: test_pc.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o test_pc test_pc.cpp $(SRC)

main_pc: main_pc.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o main_pc main_pc.cpp $(SRC)

demo_sim: demo_sim.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o demo_sim demo_sim.cpp $(SRC)

demo_render: demo_render.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o demo_render demo_render.cpp $(SRC)

clean:
	rm -f test_pc main_pc demo_sim demo_render
	rm -rf frames

.PHONY: all clean
