SOURCES = $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
#
CXXFLAGS = -ggdb -Wall -pipe -fPIC -Wno-unused-parameter -finline-functions   -O2 -Wconversion
LDFLAGS = -lpthread #-lprofiler

all : DDP
r : clean DISCD

DDP : $(OBJECTS)
	mpic++ -o ddp $(OBJECTS) $(CXXFLAGS) $(LDFLAGS)

%.o :  %.cpp
	mpic++ -c -o $@ $^ $(CXXFLAGS)

clean :
	rm -f *.o
	rm -f discd
