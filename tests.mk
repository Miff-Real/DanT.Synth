TEST_DIR = tests
CATCH2_DIR = $(TEST_DIR)/catch2
TEST_BIN_DIR = test_bin
TEST_OBJECTS_DIR = test_objects
TEST_EXECUTABLE = $(TEST_BIN_DIR)/test_runner
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.cpp, $(TEST_OBJECTS_DIR)/%.o, $(TEST_SOURCES))
TEST_CXX = g++
ARCH_FLAG := $(if $(ARCH),-arch $(ARCH),)
TEST_CXXFLAGS = -std=c++11 -Wall -Wextra -g -Wno-unused-parameter $(ARCH_FLAG)
ifeq ($(ARCH_OS), mac)
	TEST_INCLUDE_DIRS = -Isrc/dsp -I$(CATCH2_DIR) -I$(RACK_DIR) -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
	TEST_LDFLAGS += -L$(RACK_DIR) -lRack -Wl,-rpath,$(RACK_DIR) $(ARCH_FLAG)
else
	RACK_APP_DIR = "/c/Program Files/VCV/Rack2 Pro/"
	TEST_INCLUDE_DIRS = -Isrc/dsp -I$(CATCH2_DIR) -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
	TEST_LDFLAGS += -L$(RACK_APP_DIR) -lRack -lnanovg
endif

.PHONY: test clean_tests

$(TEST_EXECUTABLE): $(TEST_OBJECTS)
	@mkdir -p $(@D)
	$(TEST_CXX) $(TEST_CXXFLAGS) $^ $(TEST_LDFLAGS) -o $@
ifeq ($(ARCH_OS), mac)
	install_name_tool -change libRack.dylib $(RACK_DIR)/libRack.dylib $@
endif

$(TEST_OBJECTS_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(@D) # Create the test_objects directory if it doesn't exist
	$(TEST_CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDE_DIRS) -c $< -o $@

test: clean_tests $(TEST_EXECUTABLE)
	@echo "Running Catch2 Unit Tests..."
ifeq ($(ARCH_OS), mac)
	./$(TEST_EXECUTABLE) -s -r dant
else
	@export PATH=$(RACK_APP_DIR):$$PATH; echo "Debug PATH: $$PATH" && ./$(TEST_EXECUTABLE) -s -r dant
endif

clean_tests:
	@echo "Cleaning unit test artifacts..."
	rm -f $(TEST_EXECUTABLE)
	rm -rf $(TEST_BIN_DIR)
	rm -rf $(TEST_OBJECTS_DIR)
