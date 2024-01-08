CXX = g++
CFLAGS = -std=c++17 -O2 -Wall -g 

TARGET = test
OBJS = ../code/pool/*.cc ../code/timer/*.cc \
../code/buffer/*.cc ../code/log/*.cc ../test/test.cc \
../code/http/httprequest.cc

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)




