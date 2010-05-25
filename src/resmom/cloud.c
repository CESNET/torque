#include "cloud.h"
#include "resource.h"
#include "mom_func.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char        *path_prolog_magrathea_status;
extern char        *path_prolog_magrathea_start;
extern char        *path_prolog_magrathea_stop;
extern char *mom_host;
extern void exec_bail(job *pjob, int  code);


/* Test, whether job is cloud job, whether it have -lcluster=create */
int is_cloud_job(job *pjob)
  {
  resource        *pres=NULL;

  if (pjob)
    {
    pres = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],
                            find_resc_def(svr_resc_def, "cluster", svr_resc_size));
    }

  if (pres && (pres->rs_value.at_flags & ATR_VFLAG_SET) &&
      (strncmp(pres->rs_value.at_val.at_str,"create",6) == 0))
    {
    return 1;
    }

  return 0;
}

int cloud_set_prerun(job *pjob)
  {
  log_record(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,"cloud_set_prerun","Switching cloud job into transit state");
#if 0
  if (is_cloud_mom() && !is_cloud_job(pjob) )
    {
    log_joberr(-1,"start_cloud_exec","Not cloud job",pjob->ji_qs.ji_jobid);
    exec_bail(pjob, JOB_EXEC_FAIL1);
    return;
    }

  if (is_cloud_job(pjob))
    {
    if((pjob->ji_wattr[(int)JOB_ATR_interactive].at_flags & ATR_VFLAG_SET) && (pjob->ji_wattr[(int)JOB_ATR_interactive].at_val.at_long != 0))
      {
      log_joberr(-1,"start_cloud_exec","Cloud job cannot be interactive",pjob->ji_qs.ji_jobid);
      exec_bail(pjob, JOB_EXEC_FAIL1);
      return;
      }
#endif

    pjob->ji_qs.ji_state = JOB_STATE_TRANSIT;
    pjob->ji_wattr[(int)JOB_ATR_state].at_val.at_char = 'T';
    pjob->ji_wattr[(int)JOB_ATR_state].at_flags = ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
    pjob->ji_qs.ji_substate = JOB_SUBSTATE_PRERUN_CLOUD;
    pjob->ji_wattr[(int)JOB_ATR_substate].at_val.at_long = JOB_SUBSTATE_PRERUN_CLOUD;
    pjob->ji_wattr[(int)JOB_ATR_substate].at_flags = ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
    job_save(pjob, SAVEJOB_QUICK);
    /*update_ajob_status(pjob);*/
#if 0
    }
#endif
  return 0;
  }

int cloud_set_running(job *pjob)
  {
  extern time_t time_now;

  log_record(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,"cloud_set_running","Switching cloud job into running state");

#if 0
  if (is_cloud_job(pjob))
    {
    if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN_CLOUD)
      {
#endif

      pjob->ji_qs.ji_stime = time_now;
      pjob->ji_sampletim   = time_now;
      pjob->ji_qs.ji_state = JOB_STATE_RUNNING;
      pjob->ji_wattr[(int)JOB_ATR_state].at_val.at_char = 'R';
      pjob->ji_wattr[(int)JOB_ATR_state].at_flags = ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
      pjob->ji_qs.ji_substate = JOB_SUBSTATE_RUNNING;
      pjob->ji_wattr[(int)JOB_ATR_substate].at_val.at_long = JOB_SUBSTATE_RUNNING;
      pjob->ji_wattr[(int)JOB_ATR_substate].at_flags = ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
      job_save(pjob, SAVEJOB_QUICK);
#if 0
      if (mom_get_sample() == PBSE_NONE)
        {
        time_resc_updated = time_now;
        (void)mom_set_use(pjob);
        }
      }
  +
  +       update_ajob_status(pjob);
  +       next_sample_time = MIN_CHECK_POLL_TIME;
  +       return;
  +   }
#endif
  return 0;
  }

/* Find virtual machine and alternative, which is mapped on this
 * cloud mom.
 * Param is in format:
 * cloud_mom1=virtual_mom1[alternative];c_mom2=v_mom2[alternative]
 */
/* FIXME META From patch - review! */
char *cloud_mom_mapping(char *param,char *mom_name, char **alternative)
{ char *ret=NULL;
  char *c,*cc;
  char *look=NULL;
  char *mycopy=NULL;

  if ((param) &&
      ((mycopy=strdup(param))!=NULL)) {
      look=malloc(strlen(mom_name)+3);
      strcpy(look,mom_name);
      strcat(look,"=");

      c=strstr(mycopy,look);
      if (c) {
    c=c+strlen(look);
    cc=strchr(c,';');
    if (cc!=NULL)
        *cc='\0';
    cc=strchr(c,'[');
    if (cc!=NULL) {
        char *cb=cc+1;
        *cc='\0';
        cc=strchr(cb,']');
        if (cc!=NULL)
      *cc='\0';
        if (alternative)
      *alternative=strdup(cb);
    }
    ret=strdup(c);
  }

  if (look)
    free(look);
      if (mycopy)
    free(mycopy);
  }

  return ret;
}

/* FIXME META - from patch - review ! */
int cloud_exec(job *pjob)
  {
  int ret=0;
  int do_bail = 0;
  char *c;

  log_record(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,"cloud_exec","Executing cloud - magrathea prolog");

  c=cloud_mom_mapping(pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_val.at_str,mom_host,NULL);

  if (c)
    {
    free(c);
    log_event(PBSEVENT_JOB, PBS_EVENTCLASS_REQUEST, pjob->ji_qs.ji_jobid,"cloud_exec call");
    ret=run_pelog(PE_MAGRATHEA,path_prolog_magrathea_start,pjob,PE_IO_TYPE_NULL);
    sprintf(log_buffer,"cloud_exec, result=%d",ret);
    log_event(PBSEVENT_JOB, PBS_EVENTCLASS_REQUEST, pjob->ji_qs.ji_jobid,log_buffer);
    if (ret!=0)
      {
      exec_bail(pjob, JOB_EXEC_FAIL1);
      }
    }
  else
    {
    log_event(PBSEVENT_JOB, PBS_EVENTCLASS_REQUEST, pjob->ji_qs.ji_jobid,"cloud_exec call, no mapping");
    }

  if ((ret!=0) && (do_bail))
    {
    exec_bail(pjob, JOB_EXEC_FAIL1);
    }

  return ret;
  }
