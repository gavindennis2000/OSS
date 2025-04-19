# makefile for project 0
# compiles user.cpp and oss.cpp into two separate executables

# g++ compiler from GNU (free software foundation)
CC = g++
CFLAGS = -g

# targets and objects
TARGET1 = oss
TARGET2 = worker
OBJ1 = oss.o
OBJ2 = worker.o

# create all the targets
all: $(TARGET1) $(TARGET2)

# create the object files
$(OBJ1): oss.cpp
	$(CC) $(CFLAGS) -c oss.cpp
$(OBJ2): worker.cpp
	$(CC) $(CFLAGS) -c worker.cpp

# create the executables
$(TARGET1): $(OBJ1)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJ1)
$(TARGET2): $(OBJ2)
	$(CC) $(CFLAGS) -o $(TARGET2) $(OBJ2)

# easy-to-run test of executable
# creates 10 processes (up to 5 simultaneously) with at least
# 200ms between executions that will last between 1-5 seconds each.
test: $(TARGET1) $(TARGET2)
	./$(TARGET1) 

#debug command to open with GNU debugger
debug: $(TARGET1) $(TARGET2)
	gdb ./$(TARGET1) 

# clean command to clear build stuff
clean:
	rm ./oss
	rm ./worker
	rm -f ./*.o
	rm *.txt
