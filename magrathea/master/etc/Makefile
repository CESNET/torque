# $Id: Makefile,v 1.10 2008/08/08 10:59:14 xdenemar Exp $

-include ../../config.mak

ifeq ($(XEN),1)
    XEN_DIR := /etc/xen
endif

SUBST_IN := init.d/magrathea_master \
	    magrathea/master.example \
	    magrathea/switch.xen \
	    magrathea/switch-cpu.xen \
	    magrathea/switch.vserver \
	    magrathea/switch-cpu.vserver

CONF	 := magrathea/master.example \
	    magrathea/auto.example \
	    ../../parse-config.sh

.PHONY: install clean

install: $(SUBST_IN) $(NOSUBST)
	for dir in $(CONF_DIR) $(INIT_DIR) $(XEN_DIR); do \
	    install -m 0755 -d $(DESTDIR)$$dir; \
	done
	for switch in switch switch-cpu; do \
	    for vmm in $(VMM); do \
	        if $(FORCE_INST) || [ ! -r $(DESTDIR)$(CONF_DIR)/$$switch.$$vmm ]; then \
	            install -m 0755 magrathea/$$switch.$$vmm $(DESTDIR)$(CONF_DIR); \
	        fi \
	    done \
	done
	install -m 0755 init.d/magrathea_master $(DESTDIR)$(INIT_DIR)
	for cfg in $(CONF); do \
	    install -m 0644 $$cfg $(DESTDIR)$(CONF_DIR); \
	done
ifeq ($(XEN),1)
	install -m 0644 xen/magrathea-example $(DESTDIR)$(XEN_DIR)
endif


%: %.in
	../../subst.sh <$< >$@

clean:
	-rm -f $(SUBST_IN)

