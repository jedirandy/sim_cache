CXXFLAGS := -g -Wall -std=c++0x -lm -D_DEBUG
CXX=c++

all: cachesim

cachesim: cachesim.o cachesim_driver.o
	$(CXX) -o cachesim cachesim.o cachesim_driver.o $(CXXFLAGS)

clean:
	rm -f cachesim *.o
