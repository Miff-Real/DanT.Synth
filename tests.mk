TEST_DIR = tests
CATCH2_DIR = $(TEST_DIR)/catch2
TEST_BIN_DIR = test_bin
TEST_OBJECTS_DIR = test_objects
TEST_EXECUTABLE = $(TEST_BIN_DIR)/test_runner
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.cpp, $(TEST_OBJECTS_DIR)/%.o, $(TEST_SOURCES))
TEST_INCLUDE_DIRS = -Isrc/dsp -I$(CATCH2_DIR) -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
TEST_CXX = g++
TEST_CXXFLAGS = -std=c++11 -Wall -Wextra -g -Wno-unused-parameter
TEST_LDFLAGS = -L$(RACK_DIR) -lRack -Wl,-rpath,$(RACK_DIR)

.PHONY: test clean_tests

$(TEST_EXECUTABLE): $(TEST_OBJECTS)
	@mkdir -p $(@D)
	$(TEST_CXX) $(TEST_CXXFLAGS) $(TEST_LDFLAGS) $^ -o $@
	install_name_tool -change libRack.dylib $(RACK_DIR)/libRack.dylib $@

$(TEST_OBJECTS_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(@D) # Create the test_objects directory if it doesn't exist
	$(TEST_CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDE_DIRS) -c $< -o $@

test: clean_tests $(TEST_EXECUTABLE)
	@echo "Running Catch2 Unit Tests..."
	./$(TEST_EXECUTABLE) -s -r dant

clean_tests:
	@echo "Cleaning unit test artifacts..."
	rm -f $(TEST_EXECUTABLE)
	rm -rf $(TEST_BIN_DIR)
	rm -rf $(TEST_OBJECTS_DIR)
