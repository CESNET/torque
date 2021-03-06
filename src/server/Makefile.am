include $(top_srcdir)/buildutils/config.mk

PBS_LIBS = ../lib/Libattr/libattr.a \
	   ../lib/Libsite/libsite.a \
		 ../lib/Libutils/libutils.a \
	   ../lib/Libpbs/libtorque.la

sbin_PROGRAMS = pbs_server

pbs_server_LDADD = $(PBS_LIBS) $(GLITE_LDFLAGS) $(GLITE_LB_COMMON_NOTHR_LIBS) $(GLITE_LB_CLIENT_NOTHR_LIBS) $(GLITE_SECURITY_GSS) $(GLITE_JOBID)
AM_CFLAGS = -DPBS_SERVER_HOME=\"$(PBS_SERVER_HOME)\" -DPBS_ENVIRON=\"$(PBS_ENVIRON)\"
AM_CPPFLAGS += $(PBS_CACHE_INCLUDE) $(GLITE_CPPFLAGS) 

pbs_server_SOURCES = accounting.c array_func.c array_upgrade.c attr_recov.c \
		     dis_read.c geteusernam.c \
		     issue_request.c job_attr_def.c job_func.c job_recov.c \
		     job_route.c node_attr_def.c node_func.c \
		     node_manager.c pbsd_init.c pbsd_main.c \
		     process_request.c queue_attr_def.c queue_func.c \
		     queue_recov.c reply_send.c req_delete.c \
                     req_deletearray.c req_getcred.c \
		     req_holdjob.c req_jobobit.c req_locate.c req_manager.c \
		     req_message.c req_modify.c req_movejob.c req_quejob.c \
		     req_register.c req_rerun.c req_rescq.c req_runjob.c \
		     req_select.c req_shutdown.c req_signal.c req_stat.c \
		     req_track.c resc_def_all.c run_sched.c stat_job.c \
		     svr_attr_def.c svr_chk_owner.c svr_connect.c \
		     svr_func.c svr_jobfunc.c svr_mail.c svr_movejob.c \
		     svr_recov.c svr_resccost.c svr_task.c req_tokens.c \
         job_log.c \
		     job_qs_upgrade.c req_holdarray.c svr_format_job.c cloud.c nodespec.c

if INCLUDE_GSSAPI
pbs_server_SOURCES += req_gssauthenuser.c
endif

install-exec-hook:
	$(PBS_MKDIRS) aux || :
	$(PBS_MKDIRS) server
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qserverd$(program_suffix)$(EXEEXT)
	ln -s $(program_prefix)pbs_server$(program_suffix)$(EXEEXT) \
	   $(DESTDIR)$(sbindir)/$(program_prefix)qserverd$(program_suffix)$(EXEEXT)
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/server_priv/sbf_start
	rm -f $(DESTDIR)$(PBS_SERVER_HOME)/server_priv/sbf_stop
	cp $(top_srcdir)/meta/sbf_start $(DESTDIR)$(PBS_SERVER_HOME)/server_priv/sbf_start
	cp $(top_srcdir)/meta/sbf_stop  $(DESTDIR)$(PBS_SERVER_HOME)/server_priv/sbf_stop
	rm -f $(DESTDIR)$(sbindir)/pbs_server.start
	rm -f $(DESTDIR)$(sbindir)/pbs_server.stop
	cp $(top_srcdir)/meta/pbs_server.start $(DESTDIR)$(sbindir)/pbs_server.start
	cp $(top_srcdir)/meta/pbs_server.stop $(DESTDIR)$(sbindir)/pbs_server.stop

uninstall-hook:
	rm -f $(DESTDIR)$(PBS_ENVIRON)
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qserverd$(program_suffix)$(EXEEXT)
	rm -f $(DESDIR)$(sbindir)/pbs_server.start
	rm -f $(DESDIR)$(sbindir)/pbs_server.stop
