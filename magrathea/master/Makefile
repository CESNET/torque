# $Id: Makefile,v 1.11 2008/05/29 14:51:49 xdenemar Exp $

-include ../config.mak

LDFLAGS     := $(LDFLAGS)
CFLAGS      := $(CFLAGS) \
	       -DCONF_DIR=\"$(CONF_DIR)\" \
	       -DRUN_DIR=\"$(RUN_DIR)\" \
	       -DPBS_CACHE_API_STORE_ONLY

COMMON_INCLUDES += paths.h

MASTER_LDFLAGS	:= $(LDFLAGS) -lrt
MASTER_OBJECTS 	:= cpus.o \
		   domains.o \
		   reporter.o \
		   status.o \
		   storage.o \
		   vmm.o

MASTER_INCLUDES := $(COMMON_INCLUDES) \
		   $(MASTER_OBJECTS:%.o=%.h) \
		   ../pbs_cache/api.h
MASTER_OBJECTS	:= $(COMMON_OBJECTS) $(MASTER_OBJECTS)

VMM_LIBS	:=
VMM_OBJECTS	:=

ifeq ($(XEN),1)
    VMM_LIBS	+= -lxenctrl -lxenstore
    VMM_OBJECTS	+= xen.o
endif

ifeq ($(VSERVER),1)
    VMM_LIBS	+= -lvserver
    VMM_OBJECTS	+= vserver.o
endif

ifeq ($(STATIC),1)
    MASTER_LDFLAGS 	+= -Wl,-Bstatic $(VMM_LIBS) -Wl,-Bdynamic
else
    MASTER_LDFLAGS	+= $(VMM_LIBS)
endif
MASTER_OBJECTS		+= $(VMM_OBJECTS)


.PHONY: all install clean

all: magrathea-master magrathea-admin

magrathea-master: magrathea-master.o $(MASTER_OBJECTS) pbs_cache-api.o
	$(CC) -o $@ $^ $(MASTER_LDFLAGS)

magrathea-admin: magrathea-admin.o $(COMMON_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

install: all
	for dir in $(SBIN_DIR); do \
	    install -m 0755 -d $(DESTDIR)$$dir; \
	done
	install -m 0755 magrathea-master $(DESTDIR)$(SBIN_DIR)
	install -m 0755 magrathea-admin $(DESTDIR)$(SBIN_DIR)
	@$(MAKE) -C etc install

clean:
	-rm -f *.o magrathea-master magrathea-admin versions*
	-@$(MAKE) -C etc clean

magrathea-master.o: magrathea-master.c $(MASTER_INCLUDES)
	../versions.sh versions-$(@:%.o=%.h) $^ $(MASTER_OBJECTS:%.o=%.c) ../pbs_cache/api.c
	$(CC) $(CFLAGS) -c -o $@ $<

pbs_cache-api.o: ../pbs_cache/api.c ../pbs_cache/api.h $(COMMON_INCLUDES)
	$(CC) $(CFLAGS) -c -o $@ $<

