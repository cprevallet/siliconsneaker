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

GTKLIB=`pkg-config --cflags --libs gtk+-3.0 plplot cairo champlain-0.12 champlain-gtk-0.12 clutter-gtk-1.0 osmgpsmap-1.0`

# linker
LD=gcc
LDFLAGS=$(PTHREAD) $(GTKLIB) -export-dynamic -lm

OBJS= main.o fitwrapper.a

all: $(OBJS)	
	$(LD) -o $(TARGET) $(OBJS) $(LDFLAGS)
    
main.o: main.c fitwrapper.a
	$(CC) -c $(CCFLAGS) main.c $(GTKLIB)
    
fitwrapper.a: fitwrapper.go 
	go build -buildmode=c-archive fitwrapper.go
    
clean:
	rm -f *.o *.a $(TARGET)
