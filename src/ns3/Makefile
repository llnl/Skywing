include $(SKYNET_DIR)/src/Makefile

#--------------------- List of variables for each Module -----------------------
NS3_PREFIX = ns3-dev
NS3_SCRIPTS = \
  first.cpp
NS3_LIBS = \
  core \
	network \
	point-to-point \
	internet \
	stats \
	mpi \
	bridge \
	traffic-control \
	applications \
	config-store
NS3_INCLUDE_DIR = $(SKYNET_DIR)/utils/ns3/include/$(NS3_PREFIX)
NS3_LIB_DIR = $(SKYNET_DIR)/utils/ns3/lib64

# <Module> variables
# <MODULE>_TESTS = \
#   path_to_test_file1_without_file_extension \
#   path_to_test_file2_without_file_extension
# <MODULE>_INCLUDE_DIRS = ... list of include directories ...

#-------------------------------------------------------------------------------

#----------------------- Makefile variables ----------------------------
CPP_FLAGS = $(SKYNET_CPP_FLAGS)
CXX_FLAGS = $(SKYNET_CXX_FLAGS)
INCLUDE_FLAGS = -I$(SKYNET_INCLUDE_DIR) -I$(NS3_INCLUDE_DIR)
LIBPATH_FLAGS = -L$(NS3_LIB_DIR)
LIB_FLAGS = $(addprefix -l$(NS3_PREFIX)-,$(NS3_LIBS))
DEBUG_LIB_FLAGS = $(addsuffix -debug,$(LIB_FLAGS))
OPTIMIZED_LIB_FLAGS = $(addsuffix -optimized,$(LIB_FLAGS))
#-------------------------------------------------------------------------------

#------------------- List of files to be created for each Module ---------------

OBJECTS = $(addsuffix .o, $(NS3_SCRIPTS))
DFILES = $(addsuffix .d, $(NS3_SCRIPTS))

#-------------------------------------------------------------------------------

#--------------------- List of includes for each Module ------------------------

# Devices includes

# <Module> includes
# <MODULE>_INCLUDE_FLAGS = $(addprefix -I,$(<MODULE>_INCLUDE_DIRS))
# <MODULE>_INCLUDE_FLAGS += $(CATCH_INCLUDE_FLAG)

#-------------------------------------------------------------------------------

#----------------------------- Makefile Rules ----------------------------------

# Phony rules
.PHONY: all clean
all:
	$(MAKE) sim_debug
	$(MAKE) sim_optimized
clean:
	$(RM) sim_debug sim_optimized
	$(RM) $(OBJECTS) $(DFILES)

# implicit rule
%.cpp.o: %.cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

# explicit rules
sim_debug: $(OBJECTS)
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(OBJECTS) $(LIBPATH_FLAGS) \
	  $(DEBUG_LIB_FLAGS) -o sim_debug


#-------------------------------------------------------------------------------

#----------------------- For Makefile functionality ----------------------------
# Declare object files as secondary so make doesn't automatically delete them
.SECONDARY: $(OBJECTS)
# Include dependency lists
-include $(DFILES)
#-------------------------------------------------------------------------------
