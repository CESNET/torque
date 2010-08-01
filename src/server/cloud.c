#include "cloud.h"

#include "resource.h"
#include "assertions.h"
#include <string.h>

/* test resource if it is a cloud create request */
int is_cloud_create(resource *res)
{
  dbg_precondition(res != NULL, "This function does not accept null.");
  if (strcmp(res->rs_defin->rs_name,"cluster") == 0 &&
      strcmp(res->rs_value.at_val.at_str,"create") == 0)
    return 1;

  return 0;
}

/* determine whether job is cloud */
int is_cloud_job(job *pjob)
  {
  resource *pres = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],
                   find_resc_def(svr_resc_def, "cluster", svr_resc_size));

  if(pres != NULL && (pres->rs_value.at_flags & ATR_VFLAG_SET) &&
     is_cloud_create(pres))
    return(1);

  return(0);
}

char *switch_nodespec_to_cloud(job  *pjob, char *nodespec) { return nodespec; }
