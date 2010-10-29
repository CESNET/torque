# $Id: Makefile,v 1.14 2008/09/12 12:22:38 xdenemar Exp $

# compile-time options
IP6	  := 1
GSSAPI    := 1

# installation paths
BIN_DIR   := /usr/local/bin
SBIN_DIR  := /usr/local/sbin
RUN_DIR   := /var/run/magrathea
CONF_DIR  := /etc/magrathea
INIT_DIR  := /etc/init.d
PBS_MOM   := /usr/spool/PBS/mom_priv

# defaults
FORCE     := 0
VMM	  := auto

CC        := gcc
LDFLAGS   := $(LDFLAGS)
CFLAGS    := -g -Wall -Werror-implicit-function-declaration -Werror $(CFLAGS)

ifeq ($(IP6),1)
    CFLAGS += -DIPv6=1
endif

ifeq ($(GSSAPI),1)
    CFLAGS  += -DGSSAPI=1 $(shell krb5-config --cflags gssapi)
    LDFLAGS += $(shell krb5-config --libs gssapi)
endif

COMMON_OBJECTS  := net.o utils.o cfg.o
COMMON_OBJECTS  := $(COMMON_OBJECTS:%=../%)
COMMON_INCLUDES := $(COMMON_OBJECTS:%.o=%.h) ../config.h


ifeq ($(FORCE),1)
    FORCE_INST := true
else
    FORCE_INST := false
endif

export STATIC FORCE_INST
export CC LDFLAGS CFLAGS COMMON_OBJECTS COMMON_INCLUDES
export BIN_DIR SBIN_DIR RUN_DIR CONF_DIR INIT_DIR PBS_MOM


.PHONY: all common master slave help clean install install-master install-slave

help:
	@echo "Building:"
	@echo "    make [STATIC=1] [FORCE=1] [IP6=0] [VMM=<vmm>] [DESTDIR=dir] <target>"
	@echo "STATIC       if set to 1, VMM-specific libraries will be linked in statically"
	@echo "FORCE        if set to 1, configuration files will be overwritten"
	@echo "IP6          if set to 0, support for IPv6 will be disabled"
	@echo "VMM:"
	@echo "    auto     (default, takes everything it founds)"
	@echo "    both     (xen and vserver)"
	@echo "    xen"
	@echo "    vserver"
	@echo "DESTDIR      root directory instead of /"
	@echo "Targets:"
	@echo "    all      makes both master and slave daemons and tools"
	@echo "    master   makes master daemon and tools"
	@echo "    slave    makes slave daemon and tools"
	@echo "    clean"
	@echo "    install"
	@echo "    install-master"
	@echo "    install-slave"

all: master slave

common: $(COMMON_OBJECTS:../%=%) config.h

master slave: common
	@$(MAKE) -C $@ all

install: install-master install-slave

install-master: master
	@$(MAKE) -C master install

install-slave: slave
	@$(MAKE) -C slave install

clean:
	-rm -f *.o config.h config.mak config.tmp config.log
	-@$(MAKE) -C master clean
	-@$(MAKE) -C slave clean
	-@$(MAKE) -C pbs_cache clean

%.o: %.c %.h config.h
	$(CC) $(CFLAGS) -c -o $@ $<

config.h: configure config/functions.sh
	@-rm -f config.h config.mak config.tmp config.log 2>/dev/null
	@./configure

