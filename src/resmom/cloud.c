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
