# user defined options
CXX_FLAGS =

# project specific options
SKYNET_DIR = $(HOME)/workspace/skynet
SKYNET_INCLUDE_DIR = $(SKYNET_DIR)/include

# directory specific options
TESTS = Skynet_TestCommunicator
BOOST_INCLUDE_DIR = $(SKYNET_DIR)/utils/boost_1_68_0
DEVICES_INCLUDE_DIR = $(SKYNET_INCLUDE_DIR)/devices
BOOST_SUFFIX = _boost
TEST_PREFIX = test_

# default values for unset environment variables
CXX ?= g++
RM ?= rm -f

# add files to generate dependency lists
CPP_FLAGS += -MMD -MP

# finalize objects, dependency lists, and targets (used for "phony" rules)
BOOST_OBJECTS = $(addsuffix $(BOOST_SUFFIX).cpp.o, $(TESTS))
BOOST_DFILES = $(addsuffix $(BOOST_SUFFIX).cpp.d, $(TESTS))
BOOST_TARGETS = $(addprefix $(TEST_PREFIX), $(addsuffix $(BOOST_SUFFIX), $(TESTS)))

# finalize include flags
BOOST_INCLUDE_FLAGS = $(addprefix -I, $(BOOST_INCLUDE_DIR))
BOOST_INCLUDE_FLAGS += -I$(DEVICES_INCLUDE_DIR)

# implicit rule to compile Boost source files into objects
%$(BOOST_SUFFIX).cpp.o: %$(BOOST_SUFFIX).cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) -c $(BOOST_INCLUDE_FLAGS) $< -o $@

# implicit rule to link Boost object files into executables
$(TEST_PREFIX)%$(BOOST_SUFFIX): %$(BOOST_SUFFIX).cpp.o
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $< -o $@

# include dependency lists created during compilation
-include $(BOOST_DFILES)

# declare rules that don't correspond to actual files
.PHONY: all clean

# declare object files as secondary so make doesn't automatically delete them
.SECONDARY: $(BOOST_OBJECTS)

# make all targets
all:
	$(MAKE) $(BOOST_TARGETS)

# delete all objects, dependency lists, and executables
clean:
	$(RM) $(BOOST_OBJECTS) $(BOOST_DFILES) $(BOOST_TARGETS)
