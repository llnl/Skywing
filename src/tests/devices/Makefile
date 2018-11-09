# user defined options
BOOST_CXX_FLAGS = -Wall
CATCH_CXX_FLAGS = -Wall -std=c++11

# project specific options
SKYNET_DIR = $(HOME)/workspace/skynet
SKYNET_INCLUDE_DIR = $(SKYNET_DIR)/include

# directory specific options
TESTS = Skynet_TestCommunicator
BOOST_INCLUDE_DIR = $(SKYNET_DIR)/utils/boost_1_68_0
CATCH_INCLUDE_DIR = $(SKYNET_DIR)/utils/catch2/single_include
DEVICES_INCLUDE_DIR = $(SKYNET_INCLUDE_DIR)/devices
BOOST_SUFFIX = _boost
CATCH_SUFFIX = _catch
TEST_PREFIX = test_

# default values for unset environment variables
CXX ?= g++
RM ?= rm -f

# add files to generate dependency lists
CPP_FLAGS += -MMD -MP

# finalize objects, dependency lists, and targets
BOOST_OBJECTS = $(addsuffix $(BOOST_SUFFIX).cpp.o, $(TESTS))
BOOST_DFILES = $(addsuffix $(BOOST_SUFFIX).cpp.d, $(TESTS))
BOOST_TARGETS = $(addprefix $(TEST_PREFIX), $(addsuffix $(BOOST_SUFFIX), $(TESTS)))
CATCH_OBJECTS = catch_main.cpp.o $(addsuffix $(CATCH_SUFFIX).cpp.o, $(TESTS))
CATCH_DFILES = catch_main.cpp.d $(addsuffix $(CATCH_SUFFIX).cpp.d, $(TESTS))

# finalize include flags
BOOST_INCLUDE_FLAGS = $(addprefix -I, $(BOOST_INCLUDE_DIR))
BOOST_INCLUDE_FLAGS += -I$(DEVICES_INCLUDE_DIR)
CATCH_INCLUDE_FLAGS = $(addprefix -I, $(CATCH_INCLUDE_DIR))
CATCH_INCLUDE_FLAGS += -I$(DEVICES_INCLUDE_DIR)

# implicit rule to compile Boost source files into objects
%$(BOOST_SUFFIX).cpp.o: %$(BOOST_SUFFIX).cpp
	$(CXX) $(CPP_FLAGS) $(BOOST_CXX_FLAGS) -c $(BOOST_INCLUDE_FLAGS) $< -o $@

# implicit rule to compile Catch source files into objects
%$(CATCH_SUFFIX).cpp.o: %$(CATCH_SUFFIX).cpp
	$(CXX) $(CPP_FLAGS) $(CATCH_CXX_FLAGS) -c $(CATCH_INCLUDE_FLAGS) $< -o $@

# implicit rule to link Boost object files into executables
$(TEST_PREFIX)%$(BOOST_SUFFIX): %$(BOOST_SUFFIX).cpp.o
	$(CXX) $(CPP_FLAGS) $(BOOST_CXX_FLAGS) $< -o $@

# explicit rule to compile Catch main source file into object
catch_main.cpp.o: catch_main.cpp
	$(CXX) $(CPP_FLAGS) $(CATCH_CXX_FLAGS) -c $(CATCH_INCLUDE_FLAGS) catch_main.cpp -o catch_main.cpp.o

# explicit rule to link Catch object files into one single executable
run_catch_tests: $(CATCH_OBJECTS)
	$(CXX) $(CPP_FLAGS) $(CATCH_CXX_FLAGS) $(CATCH_OBJECTS) -o run_catch_tests

# include dependency lists created during compilation
-include $(BOOST_DFILES) $(CATCH_DFILES)

# declare rules that don't correspond to actual files
.PHONY: all boost catch clean

# declare object files as secondary so make doesn't automatically delete them
.SECONDARY: $(BOOST_OBJECTS)

# make all targets
all:
	$(MAKE) boost
	$(MAKE) catch

# make Boost targets
boost:
	$(MAKE) $(BOOST_TARGETS)

# make Catch targets
catch:
	$(MAKE) run_catch_tests

# delete all objects, dependency lists, and executables
clean:
	$(RM) $(BOOST_OBJECTS) $(BOOST_DFILES) $(BOOST_TARGETS)
	$(RM) $(CATCH_OBJECTS) $(CATCH_DFILES) run_catch_tests
