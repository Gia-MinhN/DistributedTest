CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?= -pthread

SRCS = src/main.cpp src/commands.cpp src/receiver.cpp src/sender.cpp src/node.cpp

gds: $(SRCS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SRCS)

clean:
	rm -f gds