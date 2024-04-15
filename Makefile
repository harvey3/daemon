EXEC = ./longer_daemon
OBJS = daemon.o proc_daemon.o log.o
SRC  = daemon.c proc_daemon.c log.c 

CC = arm-linux-gnueabihf-gcc
#CC = arm-none-linux-gnueabi-gcc
CFLAGS += -O2 -Wall -DCONFIG_A7
#CFLAGS += -O2 -Wall
LDFLAGS += 

all: clean $(EXEC)

$(EXEC):$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) 

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -vf $(EXEC) *.o *~
