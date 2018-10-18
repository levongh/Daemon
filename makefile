CC = g++
CPPFLAGS = -O3 -std=c++14 -Wall -Werror -Wno-unused-result
HEADERS = daemon.h
SOURCES = daemon.cpp main.cpp

OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = Daemon 

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) $(HEADERS)
	$(CC) $(OBJECTS) -o $@

$(OBJECTS): $(HEADERS)

%.o : %.cpp
	$(CC) $(CPPFLAGS) $(SOURCES) -c

clean:
	rm -rf $(EXECUTABLE) $(OBJECTS)
