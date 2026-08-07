CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
SRC = ship.cpp terrain.cpp game.cpp renderer_canvas.cpp renderer_pc.cpp storm.cpp geysers.cpp volcanoes.cpp atmosphere.cpp rings.cpp twister.cpp

all: test_pc main_pc demo_sim demo_render storm_demo geyser_demo volcano_demo titan_demo rings_demo twister_demo

test_pc: test_pc.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o test_pc test_pc.cpp $(SRC)

main_pc: main_pc.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o main_pc main_pc.cpp $(SRC)

demo_sim: demo_sim.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o demo_sim demo_sim.cpp $(SRC)

demo_render: demo_render.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o demo_render demo_render.cpp $(SRC)

storm_demo: storm_demo.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o storm_demo storm_demo.cpp $(SRC)

geyser_demo: geyser_demo.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o geyser_demo geyser_demo.cpp $(SRC)

volcano_demo: volcano_demo.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o volcano_demo volcano_demo.cpp $(SRC)

titan_demo: titan_demo.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o titan_demo titan_demo.cpp $(SRC)

rings_demo: rings_demo.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o rings_demo rings_demo.cpp $(SRC)

twister_demo: twister_demo.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o twister_demo twister_demo.cpp $(SRC)

clean:
	rm -f test_pc main_pc demo_sim demo_render storm_demo geyser_demo volcano_demo titan_demo rings_demo twister_demo
	rm -rf frames

.PHONY: all clean
