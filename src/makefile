SOURCS = main.cpp \
         daemon.cpp

OBJDIR = ../obj
OBJECTS = $(SOURCS:.cpp=.o)
MOVED_OBJECTS = $(adprefix ./$(OBJDIR)/,$(OBJECTS))
CC = g++
CPPFLAGS = -std=c++14 -c

.PHONY: clean all
all: $(OBJECTS) $(SOURCS)
	mkdir -p $(OBJDIR)
	mv *.o $(OBJDIR)

.cpp.o:
	$(CC) $(CPPFLAGS) $< -o $@

clean:
	rm -rf $(OBJDIR)
