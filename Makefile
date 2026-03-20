CXX = g++

CXXFLAGS = -std=c++11 -pthread -Iinclude
LDFLAGS = -Llib -lgreio -lpthread

SRC_DIR  = src

all: broker subscriber publisher

broker: $(SRC_DIR)/broker.cpp
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

subscriber: $(SRC_DIR)/subscriber.cpp
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

publisher: $(SRC_DIR)/publisher.cpp
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@