CXXFLAGS += -std=c++23 -fopenmp -fPIC $(shell python3.12-config --cflags) -g
CFLAGS += -fPIC -g
LDFLAGS := $(shell python3.12-config --ldflags --embed)

BUILD_DIR := build
SRC_DIR := .
CSV_DIRS := csvs csvs/measurements csvs/predictions
PLOT_DIRS := plots post_mortem
CACHE_DIRS := __pycache__ scripts/__pycache__

CSV_FILES := $(wildcard $(CSV_DIRS:%=%/*.csv))
PLOT_FILES := $(wildcard $(PLOT_DIRS)/*.png)

RM := rm -rf
MKDIR := mkdir -p

.PHONY: default
default: run

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)
$(CSV_DIRS):
	$(MKDIR) $(CSV_DIRS)

run: $(BUILD_DIR)/my_omp.so $(BUILD_DIR)/test | $(CSV_DIRS)

$(BUILD_DIR)/test: $(BUILD_DIR)/main.o
	$(CXX) -o $@ $^ $(CXXFLAGS)

$(BUILD_DIR)/my_omp.so: $(BUILD_DIR)/llsp.o $(BUILD_DIR)/my_omp.o $(BUILD_DIR)/perf.o $(BUILD_DIR)/energy.o $(BUILD_DIR)/debug_util.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -Wall -shared -g -o $@ $^ -I/usr/include/python3.12 -lpython3.12

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean_all
clean_all:
	$(RM) $(CSV_DIRS)
	$(RM) $(PLOT_DIRS)
	$(RM) $(CACHE_DIRS)
	$(RM) $(BUILD_DIR)

.PHONY: clean
clean:
	$(RM) $(PLOT_DIRS)
	$(RM) $(CSV_FILES)
