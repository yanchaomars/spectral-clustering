####################################################################################################
# 1. COMMANDS
####################################################################################################
AR            = ar
CXX           = g++ 
LD            = $(CXX)
MAKE          = make
SHELL         = bash

####################################################################################################
# 2. GLOBAL OPTIONS TO COMMANDS 
####################################################################################################
ARFLAGS			  = rs 
CXXFLAGS		  = -frounding-math -std=c++14 -fPIC -fopenmp 
LDFLAGS			  = -L/usr/lib -L/usr/local/lib -L$(BINDIR)/$(BUILD_CONF)/lib  -fopenmp 
MAKEFLAGS		  = 
SHELLFLAGS    = 

####################################################################################################
# 3. DIRECTORIES
####################################################################################################
# sources and headers
BINDIR        = bin

# compilation directories
BUILD_CONF    = ""
-include $(BINDIR)/build_conf.mk
ifeq ($(BUILD_CONF),"")
	BUILD_CONF  = unknown
endif
OBJDIR			  = $(BINDIR)/$(BUILD_CONF)/obj
RULESDIR		  = $(BINDIR)/$(BUILD_CONF)/rules
LIBDIR        = $(BINDIR)/$(BUILD_CONF)/lib

####################################################################################################
# 4. LIBRARIES
####################################################################################################
# EIGEN
EIGEN_INCLUDES    = -isystem /usr/include/eigen3                    

####################################################################################################
# 5. FLAGS
####################################################################################################
CPPFLAGS      =  
STRIPCMD      = strip --strip-debug --strip-unneeded
LDLIBS        = 
INCLUDES      = -I. $(EIGEN_INCLUDES)

ifeq ($(BUILD_CONF), release)
CPPFLAGS     += -UDEBUG -DNDEBUG -DNO_DEBUG -DEIGEN_NO_DEBUG
CXXFLAGS     += -O3 -msse2
else
STRIPCMD      = touch
CPPFLAGS     += -DDEBUG -O0 
CXXFLAGS     += -ggdb -Wall -Wextra -Wreorder -Wctor-dtor-privacy -Wwrite-strings -fno-inline -fno-inline-functions -fno-inline-small-functions
endif

# COMMAND SHORTCUT
HOSTCOMPILER  = $(CXX) $(CXXFLAGS) -c $(CPPFLAGS) $(INCLUDES)
LINKER        = $(LD)  $(LDFLAGS)     $(CPPFLAGS) $(LDLIBS)
HOSTRECIPER   = $(CXX) $(CXXFLAGS) -M $(CPPFLAGS) $(INCLUDES)

####################################################################################################
# 6. PRODUCTS
####################################################################################################
source_names  = $(notdir $(wildcard *.cc))
OBJ			  = $(source_names:%.cc=$(OBJDIR)/%.o)
RULES		  = $(source_names:%.cc=$(RULESDIR)%.d)
BIN			  = spectral_clustering

.SUFFIXES :
.SUFFIXES : .cc .d .h 
.PHONY    : debug release clean help build
.SECONDARY: 

all: build

debug:
	@mkdir -p $(BINDIR)
	@rm -rf $(BINDIR)/spectral_clustering
	@echo "BUILD_CONF = debug" > $(BINDIR)/build_conf.mk
	@echo "Build configuration \[debug\] activated and ready"
	
release:
	@mkdir -p $(BINDIR)
	@rm -rf $(BINDIR)/spectral_clustering
	@echo "BUILD_CONF = release" > $(BINDIR)/build_conf.mk
	@echo "Build configuration \[release\] activated and ready"
	
$(RULESDIR)/%.d: %.cc
	@echo -e "\033[1;30m[: > host recipe ] \033[0m$$(basename $<)"
	@mkdir -p $(RULESDIR)
	@$(HOSTRECIPER) $< -o $(RULESDIR)/$(*F).temp
	@sed -e 's,\($$*\)\.o[ :]*,\1.o $@ : ,g' \
		< $(RULESDIR)/$(*F).temp \
		> $@;
	@rm $(RULESDIR)/$(*F).temp
-include $(RULES)	

$(OBJDIR)/%.o: %.cc $(RULESDIR)/%.d 
	@echo -e "\033[1;38m[: > host compiling ] \033[0m$$(basename $@)"
	@mkdir -p $(OBJDIR)
	@$(HOSTCOMPILER) -o $@ $< 

build: $(BINDIR)/spectral_clustering

$(BINDIR)/spectral_clustering:	$(OBJ)
	@echo -e "\033[1;38m[: > building app ] \033[0m$$(basename $@)"
	@$(LINKER) $(OBJ) -o $@
	@$(STRIPCMD) $@


help:	
	@cat README.md

clean:	
	@echo "In order to avoid removing unexpected files due to bad variable definitions:"
	@echo "  remove $(BINDIR)/ manually"
