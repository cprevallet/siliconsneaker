# change application name here (executable output name)
TARGET=runplotter

# compiler
CC=gcc
# debug
DEBUG=-g
# optimisation
OPT=-O0
# warnings
WARN=-Wall

PTHREAD=-pthread

CCFLAGS=$(DEBUG) $(OPT) $(WARN) $(PTHREAD)

GTKLIB=`pkg-config --cflags --libs gtk+-3.0 plplot cairo clutter-gtk-1.0 osmgpsmap-1.0  librsvg-2.0`

# linker
LD=gcc
ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
    LDFLAGS=$(PTHREAD) $(GTKLIB) -lm 
else
    LDFLAGS=$(PTHREAD) $(GTKLIB) -export-dynamic -lm
endif

OBJS= main.o fitwrapper.a

all: $(OBJS)	
	$(LD) -o $(TARGET) $(OBJS) $(LDFLAGS)
    
main.o: main.c fitwrapper.a
	$(CC) -c $(CCFLAGS) main.c $(GTKLIB)
    
fitwrapper.a: fitwrapper.go 
	go build -buildmode=c-archive fitwrapper.go
    
clean:
	rm -f *.o *.a $(TARGET)
