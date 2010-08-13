
# Environment
MKDIR=mkdir
CP=cp
CCADMIN=CCadmin
RANLIB=ranlib
#NAME=distrib.o
NAME=paxos
OBJ=net.o msg.o node.o log.o db.o paxos.o api.o report.o
#SRC=mach_tab.c main.c
HEAD=main.h
CFLAGS=-g -O0 -Wall -Wall -fPIC -fno-stack-protector
LDFLAGS=-lpthread
OPT=-Wall -pedantic -pthread
CC=gcc

vpath %.c ../../distrib
vpath %.h ../../distrib
vpath %.y ../../distrib

# build
build: $(NAME)

$(NAME): ${OBJ}
	ar cru libdist.a net.o msg.o node.o log.o db.o paxos.o api.o report.o
	ranlib libdist.a
#	${CC} $(CFLAGS) -o $@ $^ $(LDFLAGS)

${OBJ}:${HEAD}

%.o : %.c
	gcc $(CFLAGS) -c $<

# clean
clean:
	rm -rf *.o paxos libdist.a

# all
all: paxos

# include project implementation makefile
#include nbproject/Makefile-impl.mk
