#-------------------------- Skynet project variables ---------------------------
SKYNET_DIR ?= $(HOME)/workspace/skynet
SKYNET_INCLUDE_DIR = $(SKYNET_DIR)/include
SKYNET_CXX_FLAGS = -Wall
#-------------------------------------------------------------------------------

#--------------------- List of variables for each Module -----------------------

# Devices variables
DEVICES_TESTS = \
  devices/Skynet_DummyCommunicator
DEVICES_INCLUDE_DIRS = $(SKYNET_INCLUDE_DIR)/devices

# <Module> variables
# <MODULE>_TESTS = \
#   path_to_test_file1_without_file_extension \
#   path_to_test_file2_without_file_extension
# <MODULE>_INCLUDE_DIRS = ... list of include directories ...

#-------------------------------------------------------------------------------

#----------------------- Catch & Makefile variables ----------------------------
CATCH_INCLUDE_DIR = $(SKYNET_DIR)/utils/catch2/single_include
CATCH_INCLUDE_FLAG = -I$(CATCH_INCLUDE_DIR)
CATCH_CXX_FLAGS = -std=c++11
CXX ?= g++
RM ?= rm -f
CPP_FLAGS += -MMD -MP
CXX_FLAGS = $(CATCH_CXX_FLAGS) $(SKYNET_CXX_FLAGS)
#-------------------------------------------------------------------------------

#------------------- List of files to be created for each Module ---------------

# Devices lists
DEVICES_OBJECTS = $(addsuffix _test.cpp.o, $(DEVICES_TESTS))
DEVICES_DFILES = $(addsuffix _test.cpp.d, $(DEVICES_TESTS))

# <Module> lists
# <Module>_OBJECTS = $(addsuffix _test.cpp.o, $(<MODULE>_TESTS))
# <Module>_DFILES = $(addsuffix _test.cpp.d, $(<MODULE>_TESTS))

# Master lists ***add additional module lists here***
OBJECTS = $(DEVICES_OBJECTS) catch_main.cpp.o
DFILES = $(DEVICES_DFILES) catch_main.cpp.d

#-------------------------------------------------------------------------------

#--------------------- List of includes for each Module ------------------------

# Devices includes
DEVICES_INCLUDE_FLAGS = $(addprefix -I,$(DEVICES_INCLUDE_DIRS))
DEVICES_INCLUDE_FLAGS += $(CATCH_INCLUDE_FLAG)

# <Module> includes
# <MODULE>_INCLUDE_FLAGS = $(addprefix -I,$(<MODULE>_INCLUDE_DIRS))
# <MODULE>_INCLUDE_FLAGS += $(CATCH_INCLUDE_FLAG)

#-------------------------------------------------------------------------------

#----------------------------- Makefile Rules ----------------------------------

# Phony rules
.PHONY: all devices clean
all:
	$(MAKE) run_catch_tests
devices:
	$(MAKE) run_catch_devices_tests
# <module>:
#   $(MAKE) run_catch_<module>_tests
# *** add additional module executables to clean below ***
clean:
	$(RM) run_catch_tests devices/run_catch_devices_tests
	$(RM) $(OBJECTS) $(DFILES)

# Devices rules
devices/%_test.cpp.o: devices/%_test.cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) -c $(DEVICES_INCLUDE_FLAGS) $< -o $@
run_catch_devices_tests: $(DEVICES_OBJECTS) catch_main.cpp.o
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(DEVICES_OBJECTS) catch_main.cpp.o -o devices/run_catch_devices_tests

# <Module> rules
# %_<module>_test.cpp.o: %_test.cpp
#	  $(CXX) $(CPP_FLAGS) $(CXX_FLAGS) -c $(<MODULE>_INCLUDE_FLAGS) $< -o $@
# run_catch_<module>_tests: $(<MOUDLE>_OBJECTS) catch_main.cpp.o
#		$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(<MODULE>_OBJECTS) catch_main.cpp.o -o <devices>/run_catch_<module>_tests

# Catch rules
run_catch_tests: $(OBJECTS) catch_main.cpp.o
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(OBJECTS) -o run_catch_tests
catch_main.cpp.o: catch_main.cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) -c $(CATCH_INCLUDE_FLAG) catch_main.cpp -o catch_main.cpp.o

#-------------------------------------------------------------------------------

#----------------------- For Makefile functionality ----------------------------
# Declare object files as secondary so make doesn't automatically delete them
.SECONDARY: $(OBJECTS)
# Include dependency lists
-include $(DFILES)
#-------------------------------------------------------------------------------
