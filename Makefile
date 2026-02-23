CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?= -pthread

SRCS := $(wildcard src/*.cpp)

OBJDIR := obj
OBJS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRCS))

gds: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJDIR)/%.o: src/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf gds $(OBJDIR)