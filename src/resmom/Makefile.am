include $(top_srcdir)/buildutils/config.mk

SUBDIRS = @PBS_MACH@
DIST_SUBDIRS = aix4 aix5 darwin digitalunix freebsd freebsd5 fujitsu hpux10 hpux11 \
	irix5 irix6 irix6array linux netbsd solaris5 solaris7 sunos4 unicos8 unicosmk2

AM_CPPFLAGS += -I$(top_srcdir)/src/resmom/@PBS_MACH@ -DPBS_MOM $(GLITE_CPPFLAGS) \
	       -DDEMUX=\"$(program_prefix)$(DEMUX_PATH)$(program_suffix)\" \
	       -DRCP_PATH=\"$(program_prefix)$(RCP_PATH)$(program_suffix)\" \
	       -DRCP_ARGS=\"$(RCP_ARGS)\" \
	       -DPBS_SERVER_HOME=\"$(PBS_SERVER_HOME)\" -DPBS_ENVIRON=\"$(PBS_ENVIRON)\"

PBS_LIBS = ../lib/Libattr/libattr.a \
	   ../lib/Libsite/libsite.a \
		 ../lib/Libutils/libutils.a \
	   ../lib/Libpbs/libtorque.la

EXTRA_DIST = rm_dep.h
sbin_PROGRAMS = pbs_mom pbs_demux

LDADD = @PBS_MACH@/libmommach.a $(MOMLIBS) $(PBS_LIBS) -lm $(GLITE_LDFLAGS) $(GLITE_LB_COMMON_NOTHR_LIBS) $(GLITE_LB_CLIENT_NOTHR_LIBS) $(GLITE_SECURITY_GSS) $(GLITE_JOBID)

if INCLUDE_GSSAPI
LDADD += -lkafs
endif

pbs_mom_SOURCES = catch_child.c mom_comm.c mom_inter.c mom_main.c \
		   mom_server.c prolog.c requests.c start_exec.c root_task.c \
           checkpoint.c tmsock_recov.c req_quejob.c job_func.c \
           cloud.c mom_resources.c \
           ../server/attr_recov.c ../server/dis_read.c \
		   ../server/job_attr_def.c \
		   ../server/job_recov.c  \
		   ../server/process_request.c ../server/reply_send.c \
		   ../server/resc_def_all.c \
		   ../server/job_log.c \
		   ../server/job_qs_upgrade.c ../server/nodespec.c

if BUILDCPA
pbs_mom_SOURCES += cray_cpa.c
endif

if INCLUDE_GSSAPI
pbs_mom_SOURCES += renew.c
endif


pbs_demux_SOURCES = pbs_demux.c

install-exec-hook:
	$(PBS_MKDIRS) aux || :
	$(PBS_MKDIRS) mom
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qnoded$(program_suffix)$(EXEEXT)
	ln -s $(program_prefix)pbs_mom$(program_suffix)$(EXEEXT) \
	  $(DESTDIR)$(sbindir)/$(program_prefix)qnoded$(program_suffix)$(EXEEXT)
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_start
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_status
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_stop
	rm -f $(DESTDIR)$(sbindir)/mom_environ
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/report_scratch
	cp $(top_srcdir)/meta/prolog_magrathea_start $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_start
	ln -s prolog_magrathea_start $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_stop
	ln -s prolog_magrathea_start $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_status
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prologue
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/epilogue
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/torque-gpu-mgr.sh
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-gpu-mgr.sh
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-scratch-mgr.sh
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-sw-paths.sh
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/recheck_leftover_scratch
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/check_scratch_usage
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/touch_scratch
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prologue.parallel
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/epilogue.parallel
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_lock
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_unlock
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_admin_lock
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_admin_unlock
	cp $(top_srcdir)/meta/prologue $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prologue
	cp $(top_srcdir)/meta/epilogue $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/epilogue
	cp $(top_srcdir)/meta/torque-gpu-mgr.sh $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-gpu-mgr.sh
	cp $(top_srcdir)/meta/torque-scratch-mgr.sh $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-scratch-mgr.sh
	cp $(top_srcdir)/meta/torque-sw-paths.sh $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-sw-paths.sh
	ln -s prologue $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prologue.parallel
	ln -s epilogue $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/epilogue.parallel
	cp $(top_srcdir)/meta/mom_environ $(DESTDIR)$(sbindir)/mom_environ
	cp $(top_srcdir)/meta/report_scratch $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/report_scratch
	cp $(top_srcdir)/meta/recheck_leftover_scratch $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/recheck_leftover_scratch
	cp $(top_srcdir)/meta/check_scratch_usage $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/check_scratch_usage
	cp $(top_srcdir)/meta/touch_scratch $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/touch_scratch
	cp $(top_srcdir)/meta/magrathea_lock $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_lock
	cp $(top_srcdir)/meta/magrathea_unlock $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_unlock
	cp $(top_srcdir)/meta/magrathea_admin_lock $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_admin_lock
	cp $(top_srcdir)/meta/magrathea_admin_unlock $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_admin_unlock


uninstall-hook:
	rm -f $(DESTDIR)$(PBS_ENVIRON)
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qnoded$(program_suffix)$(EXEEXT)
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_start
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_status
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prolog_magrathea_stop
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prologue
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/prologue.parallel
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/epilogue
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/epilogue.parallel
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_priv/torque-gpu-mgr.sh
	rm -f $(DESTDIR)$(sbindir)/mom_environ
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/report_scratch
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-gpu-mgr.sh
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-scratch-mgr.sh
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/torque-sw-paths.sh
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/recheck_leftover_scratch
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/check_scratch_usage
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/touch_scratch
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_lock
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_unlock
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_admin_lock
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/mom_scripts/magrathea_admin_unlock

