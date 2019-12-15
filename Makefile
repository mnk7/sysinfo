CXXFLAGS += -O3 -std=c++11 -I. -march=native

all: sysinfo

sysinfo: sysinfo.o
	$(CXX) -Wall -o $@ $(CXXFLAGS) $^
	rm -f *.o

sysinfo-debug: CXXFLAGS += -DDEBUG -g
sysinfo-debug: sysinfo

clean:
	rm -f build/*
	rm -f *.o
