# $Id: Makefile,v 1.5 2008/05/21 15:56:16 xdenemar Exp $

-include ../config.mak

LDFLAGS     := $(LDFLAGS)
CFLAGS      := $(CFLAGS) \
	       -DCONF_DIR=\"$(CONF_DIR)\" \
	       -DRUN_DIR=\"$(RUN_DIR)\"

.PHONY: all install clean

all: magrathea-slave magrathea-cmd

magrathea-cmd: magrathea-cmd.o $(COMMON_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

magrathea-slave: magrathea-slave.o $(COMMON_OBJECTS)
	$(CC) -o $@ $^ -pthread $(LDFLAGS)

install: all
	for dir in $(SBIN_DIR); do \
	    install -m 0755 -d $(DESTDIR)$$dir; \
	done
	install -m 0755 magrathea-slave $(DESTDIR)$(SBIN_DIR)
	install -m 0755 magrathea-cmd $(DESTDIR)$(SBIN_DIR)
	@$(MAKE) -C etc install

clean:
	-rm -f *.o magrathea-slave magrathea-cmd versions*
	-@$(MAKE) -C etc clean

magrathea-cmd.o: magrathea-cmd.c $(COMMON_INCLUDES) magrathea-slave.h
	../versions.sh versions-$(@:%.o=%.h) $^ $(COMMON_OBJECTS:%.o=%.c)
	$(CC) $(CFLAGS) -c -o $@ $<

magrathea-slave.o: magrathea-slave.c $(COMMON_INCLUDES) magrathea-slave.h
	../versions.sh versions-$(@:%.o=%.h) $^ $(COMMON_OBJECTS:%.o=%.c)
	$(CC) $(CFLAGS) -pthread -c -o $@ $<

