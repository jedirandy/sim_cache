#DEBUG=-D_DEBUG
CXXFLAGS := -g -Wall -std=c++0x -o3 -lm $(DEBUG) 
CXX=c++
all: cachesim

cachesim: cachesim.o cachesim_driver.o
	$(CXX) -o cachesim cachesim.o cachesim_driver.o

clean:
	rm -f cachesim *.o
