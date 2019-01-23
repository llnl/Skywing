include $(SKYNET_DIR)/src/Makefile
#--------------------- List of variables for each Module -----------------------

# Devices classes
DEVICES_TESTS = \
  devices/Skynet_SocketGateway

# Heartbeat classes
HEARTBEAT_TESTS = \
	heartbeat/Skynet_DeviceIdRegistry \
  heartbeat/Skynet_Heart

# <Module> variables
# <MODULE>_TESTS = \
#   path_to_test_file1_without_file_extension \
#   path_to_test_file2_without_file_extension

#-------------------------------------------------------------------------------

#---------------------------- Catch variables ----------------------------------
CATCH_INCLUDE_DIR = $(SKYNET_DIR)/utils/catch2/single_include
# Set CATCH_CXX_FLAGS to -std=c++11 if no standard specified in SKYNET_CXX_FLAGS
CATCH_CXX_FLAGS =
#-------------------------------------------------------------------------------

#---------------------------- Makefile variables -------------------------------
CPP_FLAGS = $(SKYNET_CPP_FLAGS)
CXX_FLAGS = $(CATCH_CXX_FLAGS) $(SKYNET_CXX_FLAGS)
INCLUDE_FLAGS = -I$(SKYNET_INCLUDE_DIR) -I$(CATCH_INCLUDE_DIR)
LIB_FLAGS = -l$(SKYNET_LIBS)
#-------------------------------------------------------------------------------

#------------------- List of files to be created for each Module ---------------

# Devices lists
DEVICES_OBJECTS = $(addsuffix _test.cpp.o, $(DEVICES_TESTS))
DEVICES_DFILES = $(addsuffix _test.cpp.d, $(DEVICES_TESTS))

# Heartbeat lists
HEARTBEAT_OBJECTS = $(addsuffix _test.cpp.o, $(HEARTBEAT_TESTS))
HEARTBEAT_DFILES = $(addsuffix _test.cpp.d, $(HEARTBEAT_TESTS))

# <Module> lists
# <Module>_OBJECTS = $(addsuffix _test.cpp.o, $(<MODULE>_TESTS))
# <Module>_DFILES = $(addsuffix _test.cpp.d, $(<MODULE>_TESTS))

# Master lists ***add additional module lists here***
OBJECTS = $(DEVICES_OBJECTS) $(HEARTBEAT_OBJECTS) catch_main.cpp.o
DFILES = $(DEVICES_DFILES) $(HEARTBEAT_DFILES) catch_main.cpp.d

#-------------------------------------------------------------------------------

#----------------------------- Makefile Rules ----------------------------------

# Phony rules
.PHONY: all devices clean
all:
	$(MAKE) run_catch_tests
devices:
	$(MAKE) run_catch_devices_tests
heartbeat:
	$(MAKE) run_catch_heartbeat_tests
# <module>:
#   $(MAKE) run_catch_<module>_tests
# *** add additional module executables to clean below ***
clean:
	$(RM) run_catch_tests
	$(RM) devices/run_catch_devices_tests
	$(RM) heartbeat/run_catch_heartbeat_tests
	$(RM) $(OBJECTS) $(DFILES)

# Devices rules
devices/%_test.cpp.o: devices/%_test.cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@
run_catch_devices_tests: $(DEVICES_OBJECTS) catch_main.cpp.o
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(LIB_FLAGS) \
	$(DEVICES_OBJECTS) catch_main.cpp.o -o devices/run_catch_devices_tests

# Devices rules
heartbeat/%_test.cpp.o: heartbeat/%_test.cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@
run_catch_heartbeat_tests: $(HEARTBEAT_OBJECTS) catch_main.cpp.o
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(LIB_FLAGS) \
	$(HEARTBEAT_OBJECTS) catch_main.cpp.o -o heartbeat/run_catch_heartbeat_tests

# <Module> rules
# %_<module>_test.cpp.o: %_test.cpp
#	  $(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@
# run_catch_<module>_tests: $(<MOUDLE>_OBJECTS) catch_main.cpp.o
#		$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(LIB_FLAGS) \
#   $(<MODULE>_OBJECTS) catch_main.cpp.o -o <devices>/run_catch_<module>_tests

# Catch rules
run_catch_tests: $(OBJECTS) catch_main.cpp.o
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(LIB_FLAGS) $(OBJECTS) -o run_catch_tests
catch_main.cpp.o: catch_main.cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(INCLUDE_FLAGS) -c catch_main.cpp -o catch_main.cpp.o

#-------------------------------------------------------------------------------

#----------------------- For Makefile functionality ----------------------------
# Declare object files as secondary so make doesn't automatically delete them
.SECONDARY: $(OBJECTS)
# Include dependency lists
-include $(DFILES)
#-------------------------------------------------------------------------------
