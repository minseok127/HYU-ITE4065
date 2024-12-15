.SUFFIXES: .cpp .o

CXX=g++-10

SRCS:=$(wildcard src/*.cpp)
OBJS:=$(SRCS:.cpp=.o)

CXXFLAGS+= -std=c++20

TARGET=run

$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

clean:
	rm $(TARGET) $(OBJS)

test:
	./run 1
	./run 2
	./run 4
	./run 8
	./run 16
	./run 32
