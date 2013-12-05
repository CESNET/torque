include $(top_srcdir)/buildutils/config.mk

PBS_LIBS = ../lib/Libpbs/libtorque.la

sbin_PROGRAMS = pbs_sched
schedprivdir = $(PBS_SERVER_HOME)/sched_priv
schedpriv_DATA = sched_config resource_group holidays dedicated_time resources.def resources_matlab.def resources_ncbr.def

EXTRA_DIST = $(schedpriv_DATA) README

INCLUDES = $(PBS_CACHE_INCLUDE)

AM_CFLAGS = -DPBS_SERVER_HOME=\"$(PBS_SERVER_HOME)\" -DPBS_ENVIRON=\"$(PBS_ENVIRON)\"

pbs_sched_LDADD = $(PBS_LIBS)
pbs_sched_SOURCES = pbs_sched.cpp check.cpp dedtime.cpp fairshare.cpp fifo.cpp globals.cpp \
		    job_info.cpp misc.cpp node_info.cpp parse.cpp prev_job_info.cpp \
		    prime.cpp queue_info.cpp server_info.cpp sort.cpp state_count.cpp \
		    check.h config.h constant.h data_types.h dedtime.h \
		    fairshare.h fifo.h globals.h job_info.h misc.h node_info.h \
		    parse.h prev_job_info.h prime.h queue_info.h server_info.h \
		    sort.h state_count.h \
	        token_acct.h token_accounting.cpp \
            nodespec.cpp nodespec_sch.h site_pbs_cache.cpp \
            ../lib/Libsite/site_pbs_cache_common.c JobInfo.cpp DynamicResource.cpp NodeInfo.cpp SchedulerRealm.h SchedulerRealm.cpp JobLog.h JobLog.cpp ConnectionMgr.h ConnectionMgr.cpp \
            utility.h utility.cpp RescInfoDb.h RescInfoDb.cpp NodeState.cpp NodeFilters.cpp NodeSort.cpp ServerInfo.cpp

install-exec-hook:
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qschedd$(program_suffix)$(EXEEXT)
	ln -s $(program_prefix)pbs_sched$(program_suffix)$(EXEEXT) \
	  $(DESTDIR)$(sbindir)/$(program_prefix)qschedd$(program_suffix)$(EXEEXT)

uninstall-hook:
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qschedd$(program_suffix)$(EXEEXT)

