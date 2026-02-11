CXX ?= g++-11
CXXFLAGS += -Wextra -Wall -std=c++17 -O2

gds: src/main.cpp src/commands.cpp
	$(CXX) $(CXXFLAGS) -o gds src/main.cpp src/commands.cpp src/receiver.cpp src/sender.cpp src/node.cpp

clean:
	rm -f gds
