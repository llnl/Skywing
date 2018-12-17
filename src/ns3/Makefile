include $(SKYNET_DIR)/src/Makefile

#-------------------- List of variables for all simulators ---------------------
NS3_PREFIX = ns3-dev
NS3_INCLUDE_DIR = $(SKYNET_DIR)/utils/ns3/include/$(NS3_PREFIX)
NS3_LIB_DIR = $(SKYNET_DIR)/utils/ns3/lib64
#-------------------------------------------------------------------------------

#--------------- List of variables specific to each simulator ------------------
UDP_ECHO_SRC = udp_echo.cpp
UDP_ECHO_LIBS = \
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

SKYNET_STARTUP_SRC = \
	skynet_startup.cpp \
	HeartHelper.cpp \
	Ns3_Heart.cpp
SKYNET_STARTUP_LIBS = \
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
#-------------------------------------------------------------------------------

#---------------- List of files to be created for each simulator ---------------
UDP_ECHO_OBJECTS = $(addsuffix .o, $(UDP_ECHO_SRC))
UDP_ECHO_DFILES = $(addsuffix .d, $(UDP_ECHO_SRC))

SKYNET_STARTUP_OBJECTS = $(addsuffix .o, $(SKYNET_STARTUP_SRC))
SKYNET_STARTUP_DFILES = $(addsuffix .d, $(SKYNET_STARTUP_SRC))
#-------------------------------------------------------------------------------

#--------------------- List of lib flags for each simulator --------------------
TMP = $(addprefix $(NS3_PREFIX)-, $(UDP_ECHO_LIBS))
UDP_ECHO_LIB_FLAGS = $(addprefix -l, $(TMP))
UDP_ECHO_DEBUG_LIB_FLAGS = $(addsuffix -debug,$(UDP_ECHO_LIB_FLAGS))
UDO_ECHO_OPTIMIZED_LIB_FLAGS = $(addsuffix -optimized,$(UDP_ECHO_LIB_FLAGS))

TMP = $(addprefix $(NS3_PREFIX)-, $(SKYNET_STARTUP_LIBS))
SKYNET_STARTUP_LIB_FLAGS = $(addprefix -l, $(TMP))
SKYNET_STARTUP_DEBUG_LIB_FLAGS = $(addsuffix -debug,$(SKYNET_STARTUP_LIB_FLAGS))
SKYNET_STARTUP_OPTIMIZED_LIB_FLAGS = $(addsuffix -optimized,$(SKYNET_STARTUP_LIB_FLAGS))

#-------------------------------------------------------------------------------

#--------------------------- Makefile variables --------------------------------
CPP_FLAGS = $(SKYNET_CPP_FLAGS)
CXX_FLAGS = $(SKYNET_CXX_FLAGS)
INCLUDE_FLAGS = -I$(SKYNET_INCLUDE_DIR) -I$(NS3_INCLUDE_DIR)
LIBPATH_FLAGS = -L$(NS3_LIB_DIR)
OBJECTS = $(UDP_ECHO_OBJECTS) $(SKYNET_STARTUP_OBJECTS)
DFILES = $(UDP_ECHO_DFILES) $(SKYNET_STARTUP_DFILES)
#-------------------------------------------------------------------------------

#----------------------------- Makefile Rules ----------------------------------

# Phony rules
.PHONY: all clean
all:
	$(MAKE) udp_echo_debug udp_echo_optimized
	$(MAKE) skynet_startup_debug skynet_startup_optimized
clean:
	$(RM) udp_echo_debug udp_echo_optimized
	$(RM) skynet_startup_debug skynet_startup_optimized
	$(RM) $(OBJECTS) $(DFILES)

# implicit rule
%.cpp.o: %.cpp
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

# explicit rules
udp_echo_debug: $(UDP_ECHO_OBJECTS)
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(LIBPATH_FLAGS) \
	$(UDP_ECHO_DEBUG_LIB_FLAGS) $^ -o $@

skynet_startup_debug: $(SKYNET_STARTUP_OBJECTS)
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) $(LIBPATH_FLAGS) \
	$(SKYNET_STARTUP_DEBUG_LIB_FLAGS) $^ -o $@

#-------------------------------------------------------------------------------

#----------------------- For Makefile functionality ----------------------------
# Declare object files as secondary so make doesn't automatically delete them
.SECONDARY: $(OBJECTS)
# Include dependency lists
-include $(DFILES)
#-------------------------------------------------------------------------------
