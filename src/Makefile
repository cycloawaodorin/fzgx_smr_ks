CXX = g++
CXXWFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -Wold-style-cast
CXXFLAGS = -c -O3 -std=c++23
INCLUDES = 
LD = g++
LDFLAGS = -shared -Wl,--dll,--enable-stdcall-fixup
LIBS = -static -lstdc++ -lwinpthread -lGdi32
RM = rm -f
SH = bash

TARGET = fzgx_smr_ks.auo
OBJ = fzgx_smr_ks.o
SRC = fzgx_smr_ks.cpp
SUBSRCS = weights0.cpp
HDRS = output.hpp resource_definition.h version.hpp
RES = fzgx_smr_ks.res
DEF = fzgx_smr_ks.def

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

fzgx_smr_ks.res: resource_definition.h

clean:
	$(RM) $(TARGET) $(OBJ) $(RES)

release:
	make all
	cp -f $(TARGET) ../
	$(SH) makezip.sh fzgx_smr_ks
	make clean
