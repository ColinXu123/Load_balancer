# Load Balancer Simulation - Makefile
#   make          build ./loadbalancer
#   make run      20 servers, 100,000 cycles, quiet
#   make logs     regenerate the sample logs in logs/
#   make docs     generate Doxygen HTML in docs/doxygen/html
#   make clean    remove build output

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
SRC_DIR  := src
OBJ_DIR  := build
TARGET   := loadbalancer

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all run logs docs clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(TARGET)
	./$(TARGET) -s 20 -t 100000 -q

logs: $(TARGET)
	mkdir -p logs
	./$(TARGET) -s 20 -t 100000 -q --switch --no-color --log logs/summary_switch_20x100000.log > /dev/null
	./$(TARGET) -s 20 -t 100000 -q --single --no-color --log logs/summary_single_20x100000.log > /dev/null
	./$(TARGET) -s 20 -t 100000 -v --switch --no-color --log logs/verbose_switch_20x100000.log > /dev/null

docs:
	mkdir -p docs/doxygen
	doxygen Doxyfile

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

-include $(DEPS)
