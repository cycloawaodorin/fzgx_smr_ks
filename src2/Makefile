CXX = g++
CXXWFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -Wold-style-cast
CXXFLAGS = -c -O3 -std=c++23
INCLUDES = 
LD = g++
LDFLAGS = -shared -Wl,--dll,--enable-stdcall-fixup
LIBS = -static -lstdc++ -lwinpthread -lGdi32
RM = rm -f
SH = bash

LIBNAME = fzgx_smr_ks
TARGET = $(LIBNAME).auo2
OBJ = $(LIBNAME).o
SRC = $(LIBNAME).cpp
SUBSRCS = weights0.cpp
HDRS = output2.hpp config2.hpp resource_definition.h version.hpp
RES = $(LIBNAME).res
DEF = $(LIBNAME).def

.SUFFIXES: .cpp .o
.SUFFIXES: .rc .res

all: $(TARGET)

$(TARGET): $(OBJ) $(RES) $(DEF)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJ) $(RES) $(DEF) $(LIBS)
	strip $(TARGET)

$(OBJ): $(SRCS) $(HDRS) $(SUBSRCS)
	$(CXX) $(CXXWFLAGS) $(CXXFLAGS) $(INCLUDES) -o $(OBJ) $(SRC)

.rc.res:
	windres --input-format=rc --output-format=coff -o $@ $<

$(RES): resource_definition.h

clean:
	$(RM) $(TARGET) $(OBJ) $(RES)

release:
	make all
	cp -f $(TARGET) ../
	$(SH) makezip.sh $(LIBNAME)
	make clean
