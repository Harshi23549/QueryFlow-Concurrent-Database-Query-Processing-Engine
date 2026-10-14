CXX := g++
CXXFLAGS := -std=c++17 -pthread -Wall -Wextra -O2 -Iinclude

APP_TARGET := build/QueryFlow
SCHED_TEST := build/SchedulerTests
CONC_TEST := build/ConcurrencyTests

APP_SRCS := src/app/main.cpp \
    src/scheduler/Scheduler.cpp \
    src/queue/SharedQueryQueue.cpp \
    src/worker/WorkerPool.cpp \
    src/database/DatabaseManager.cpp \
    src/database/MockDatabaseManager.cpp \
    src/manager/QueryManager.cpp \
    src/metrics/QueryMetrics.cpp \
    src/config/QueryFlowConfig.cpp

SCHED_TEST_SRCS := tests/scheduler_test.cpp src/scheduler/Scheduler.cpp
CONC_TEST_SRCS := tests/concurrency_test.cpp src/queue/SharedQueryQueue.cpp src/worker/WorkerPool.cpp src/database/MockDatabaseManager.cpp

.PHONY: all run test clean

all: $(APP_TARGET)

run: $(APP_TARGET)
	./$(APP_TARGET)

$(APP_TARGET): $(APP_SRCS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(APP_SRCS)

$(SCHED_TEST): $(SCHED_TEST_SRCS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(SCHED_TEST_SRCS)

$(CONC_TEST): $(CONC_TEST_SRCS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(CONC_TEST_SRCS)

test: $(SCHED_TEST) $(CONC_TEST)
	./$(SCHED_TEST)
	./$(CONC_TEST)

clean:
	rm -rf build
