CXX      := g++
CXXFLAGS := -std=c++17 -pthread -Wall -Wextra -O2
TARGET   := concurrency_test

SRCS := ConcurrencyTest.cpp SharedQueryQueue.cpp WorkerPool.cpp MockDatabaseManager.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all run clean tsan

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build with ThreadSanitizer to check for data races.
tsan: CXXFLAGS += -fsanitize=thread -g -O1
tsan: clean $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
