#include "cloud.h"
#include <string.h>
#include "resource.h"

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
#if 0
  if (is_cloud_job(pjob))
    {
    if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN_CLOUD)
      {
#endif
    extern time_t time_now;
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


int cloud_exec(job *pjob)
  {
  return 0;
  }
