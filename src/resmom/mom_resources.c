#include "mom_resources.h"
#include "nodespec.h"
#include "resource.h"
#include "pbs_job.h"
#include <stdio.h>
#include <ctype.h>

extern char mom_host[];

/** \brief Export variable
 *
 * Either add another entry into a supplied vtable (used in start_process), or directly export the variable using setenv() (used in run_pelog).
 *
 * @param name Name of the variable
 * @param value Value of the variable
 * @param vtable Used when this is export into a controlled sub-process, otherwise NULL
 */
void export_variable(const char* name, const char* value, struct var_table *vtable)
  {
  if (vtable != NULL)
    bld_env_variables(vtable,(char*)name,(char*)value);
  else
    setenv(name,value,1);
  }

void set_resource_vars(job *pjob, struct var_table *vtable)
  {
  resource *pres;
  char buf_val[256] = { 0 };
  char buf_name[256] = { 0 };

  pres = (resource *)GET_NEXT(pjob->ji_wattr[(int)JOB_ATR_total_resources].at_val.at_list);

  while (pres != NULL)

    {
    switch (pres->rs_defin->rs_type)
      {
      case ATR_TYPE_LONG:
          sprintf(buf_val,"%ld",pres->rs_value.at_val.at_long);
        break;
      case ATR_TYPE_LL:
          sprintf(buf_val,"%lld",pres->rs_value.at_val.at_ll);
        break;
      case ATR_TYPE_STR:
          sprintf(buf_val,"%s",pres->rs_value.at_val.at_str);
        break;
      case ATR_TYPE_SIZE:
          sprintf(buf_val,"%ld",(pres->rs_value.at_val.at_size.atsv_num << pres->rs_value.at_val.at_size.atsv_shift));
        break;
      }

    sprintf(buf_name,"%s%s","TORQUE_RESC_TOTAL_",pres->rs_defin->rs_name);
    for (int i = 0; buf_name[i] != '\0'; i++)
      {
      buf_name[i] = toupper(buf_name[i]);
      }

    export_variable(buf_name,buf_val,vtable);

    pres = (resource *)GET_NEXT(pres->rs_link);
    }

  pars_spec *spec = parse_nodespec(pjob->ji_wattr[JOB_ATR_sched_spec].at_val.at_str);

  pars_spec_node *node = find_node_in_spec(spec,mom_host);

  if (node == NULL) /* FIXME - cloud jobs on cloud nodes don't have record in nodespec */
    {
    free_parsed_nodespec(spec);
    return;
    }

  sprintf(buf_val,"%u",node->procs);
  export_variable("TORQUE_RESC_PROC",buf_val,vtable);

  sprintf(buf_val,"%llu",node->mem*1024);
  export_variable("TORQUE_RESC_MEM",buf_val,vtable);

  sprintf(buf_val,"%llu",node->vmem*1024);
  export_variable("TORQUE_RESC_VMEM",buf_val,vtable);

  if (node->scratch_type == ScratchLocal)
    sprintf(buf_val,"/scratch/%s/job_%s",pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str,pjob->ji_qs.ji_jobid);
  else if (node->scratch_type == ScratchSSD)
    sprintf(buf_val,"/scratch.ssd/%s/job_%s",pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str,pjob->ji_qs.ji_jobid);
  else if (node->scratch_type == ScratchShared)
    sprintf(buf_val,"/scratch.shared/%s/job_%s",pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str,pjob->ji_qs.ji_jobid);

  if (node->scratch_type != ScratchNone)
    {
    export_variable("SCRATCHDIR",buf_val,vtable);
    export_variable("SCRATCH",buf_val,vtable);

    if (node->scratch_type == ScratchLocal)
      export_variable("TORQUE_RESC_SCRATCH_TYPE","local",vtable);
    else if (node->scratch_type == ScratchShared)
      export_variable("TORQUE_RESC_SCRATCH_TYPE","shared",vtable);
    else if (node->scratch_type == ScratchSSD)
      export_variable("TORQUE_RESC_SCRATCH_TYPE","ssd",vtable);

    sprintf(buf_val,"%llu",node->scratch*1024);
    export_variable("TORQUE_RESC_SCRATCH_VOLUME",buf_val,vtable);
    }
  else
    {
    /*
     * special case when using ":first" option
     * if the scratch is shared, set scratch directory and other variables for all nodes, not just the first one
     */
    if (spec->nodes->scratch_type == ScratchShared)
      {
      sprintf(buf_val,"/scratch.shared/%s/job_%s",pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str,pjob->ji_qs.ji_jobid);

      export_variable("SCRATCHDIR",buf_val,vtable);
      export_variable("SCRATCH",buf_val,vtable);
      export_variable("TORQUE_RESC_SCRATCH_TYPE","shared",vtable);

      sprintf(buf_val,"%llu",spec->nodes->scratch*1024);
      export_variable("TORQUE_RESC_SCRATCH_VOLUME",buf_val,vtable);
      }
    }

  resource res;
  pars_prop *prop = node->properties;
  while (prop != NULL)
    {
    if (prop->value != NULL)
    for (int i = 0; i < svr_resc_size; i++)
      {
      sprintf(buf_name,"TORQUE_RESC_%s",prop->name);

      int len = strlen(buf_name);
      for (int i = 0; i < len; i++)
      { buf_name[i] = toupper(buf_name[i]); }

      if (strcmp(svr_resc_def[i].rs_name,prop->name) == 0)
        {
        svr_resc_def[i].rs_decode(&res.rs_value,"","",prop->value);
        switch (svr_resc_def[i].rs_type)
          {
          case ATR_TYPE_LONG:
            sprintf(buf_val,"%ld",res.rs_value.at_val.at_long);
            break;
          case ATR_TYPE_LL:
            sprintf(buf_val,"%lld",res.rs_value.at_val.at_ll);
            break;
          case ATR_TYPE_SIZE:
            sprintf(buf_val,"%ld",res.rs_value.at_val.at_size.atsv_num <<
                                  res.rs_value.at_val.at_size.atsv_shift);
            break;
          case ATR_TYPE_STR:
            strcpy(buf_val,prop->value);
            break;
          }

        export_variable(buf_name,buf_val,vtable);
        }
      }
    prop = prop->next;
    }

  free_parsed_nodespec(spec);
  }

void read_environ_script(job *pjob, struct var_table *vtable)
  {
  char *cmd = malloc(strlen(ENVIRONGEN) + 1 +
                     strlen(pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str) + 1 +
                     strlen(pjob->ji_qs.ji_jobid) + 1);
  if (cmd == NULL) return; /* TODO */

  sprintf(cmd,"%s %s %s",ENVIRONGEN,pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str,pjob->ji_qs.ji_jobid);

  FILE *input = popen(cmd,"r");
  if (input == NULL) /* couldn't start script */
    {
    log_err(-1, "read_environ_script", "cmd");
    return;
    }

  char *var = NULL;
  size_t varl = 0;

  while (getline(&var,&varl,input) != -1)
    {
    char *val = NULL;
    size_t vall = 0;

    if (getline(&val,&vall,input) == -1)
      {
      free(var);
      break;
      }

    var[strlen(var)-1] = '\0';
    val[strlen(val)-1] = '\0';

    export_variable(var,val,vtable);

    free(var);
    free(val);

    var = NULL;
    varl = 0;
    }

  pclose(input);
  }
