CC = g++
SUBDIRS    = src
OBJDIR     = obj
BINDIR     = bin
DOXY       = ./docs/doxy
OBJECTS    = $(SOURCES:.cpp:=.o)
EXECUTABLE = Daemon 
CPPFLAGS   = -O3 -std=c++14
LDFLAGS    =

MOVED_OBJECTS =  $(addprefix $(OBJDIR)/, $(OBJECTS))

.PHONY : all clean doxygen

all: $(OBJECTS)
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)
	mkdir -p $(DOXY)
	$(foreach subdir, $(SUBDIRS), cd $(subdir); make all; cd ../;)
	$(CC) $(OBJDIR)/* $(LDFLAGS) -o $(EXECUTABLE)
	mv $(EXECUTABLE) $(BINDIR)

doxygen:
	@doxygen	./docs/Doxyfile
clean:
	rm -rf $(BINDIR) $(OBJDIR) $(DOXY)
	$(foreach subdir,$(SUBDIRS), cd $(subdir); make clean; cd ../;)
