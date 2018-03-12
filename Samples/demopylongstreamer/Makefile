# Makefile for demopylongstreamer
.PHONY: all clean

# The program to build
NAME       := demopylongstreamer
CLASS1	   := ../../InstantCameraAppSrc/CInstantCameraAppSrc
CLASS2     := CPipelineHelper

# Installation directories for pylon
PYLON_ROOT ?= /opt/pylon5
DIR ?= /usr/include

# Build tools and flags
LD         := $(CXX)
CPPFLAGS   := $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0) $(shell $(PYLON_ROOT)/bin/pylon-config --cflags) -std=c++11
CXXFLAGS   := #e.g., CXXFLAGS=-g -O0 for debugging
LDFLAGS    := $(shell $(PYLON_ROOT)/bin/pylon-config --libs-rpath) 
LDLIBS     := $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0) $(shell $(PYLON_ROOT)/bin/pylon-config --libs)

# Rules for building
all: $(NAME)

$(NAME): $(NAME).o $(CLASS1).o $(CLASS2).o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(NAME).o: $(NAME).cpp $(CLASS1).cpp $(CLASS2).cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(NAME).o $(CLASS1).o $(CLASS2).o $(NAME)
