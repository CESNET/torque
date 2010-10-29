# $Id: Makefile,v 1.12 2008/08/08 10:59:14 xdenemar Exp $

PBSFILES := pbs_mom/epilogue pbs_mom/epilogue_meta \
	    pbs_mom/prologue pbs_mom/prologue_meta \
	    pbs_mom/job_cpu.sh

SUBST_IN := init.d/magrathea_slave \
	    magrathea/slave.example \
	    magrathea/master-cmd \
	    $(PBSFILES)

CONF	 := magrathea/slave.example ../../parse-config.sh

.PHONY: install clean

install: $(SUBST_IN)
	for dir in $(CONF_DIR) $(INIT_DIR) $(PBS_MOM); do \
	    install -m 0755 -d $(DESTDIR)$$dir; \
	done
	if $(FORCE_INST) || [ ! -r $(DESTDIR)$(CONF_DIR)/master-cmd ]; then \
	    install -m 0755 magrathea/master-cmd $(DESTDIR)$(CONF_DIR); \
	fi
	install -m 0755 init.d/magrathea_slave $(DESTDIR)$(INIT_DIR)
	for file in $(PBSFILES); do \
    	    install -m 0755 $$file $(DESTDIR)$(PBS_MOM); \
	done
	for cfg in $(CONF); do \
	    install -m 0644 $$cfg $(DESTDIR)$(CONF_DIR); \
	done

$(SUBST_IN): %: %.in
	../../subst.sh <$< >$@

clean:
	-rm -f $(SUBST_IN)

