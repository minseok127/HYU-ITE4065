.SUFFIXES: .cpp .o

CXX=g++

SRCS:=$(wildcard src/*.cpp)
OBJS:=$(SRCS:.cpp=.o)

CXXFLAGS+= -std=c++17

TARGET=run

$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

clean:
	rm $(TARGET) $(OBJS)
	rm thread*.txt

trunc:
	rm thread*.txt