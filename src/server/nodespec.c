#include "pbs_job.h"
#include "nodespec.h"
#include "resource.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/** Recalculate real nodespec and total resources
 *
 * real nodespec   - nodespec containing all resources
 * total resources - total amount of counted resources
 */
void regenerate_total_resources(job * pjob)
  {
  pars_spec *spec = NULL;
  resource_def *rd;
  resource *rs;
  pars_spec_node *node = NULL;
  pars_prop *prop = NULL;
  unsigned long long mem = 0, vmem = 0, scratch = 0;
  int i, ret;

  /* Step (1)
   * - find nodespec and parse
   */
  if ((pjob->ji_wattr[(int)JOB_ATR_resource].at_flags & ATR_VFLAG_SET) != 0)
    {
    if ((rd = find_resc_def(svr_resc_def,"nodes",svr_resc_size)) == NULL)
      return;

    if ((rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],rd)) == NULL)
      return;

    if ((rs->rs_value.at_flags & ATR_VFLAG_SET) == 0)
      return;

    if (rs->rs_value.at_val.at_str == NULL)
      return;

    spec = parse_nodespec(rs->rs_value.at_val.at_str);

    if (spec == NULL)
      return;
    }
  else
    {
    return;
    }

  /* Step (2)
   * - free up previous total resources
   */
  /* cleanup any previous values */
  job_attr_def[(int)JOB_ATR_total_resources].
    at_free(&pjob->ji_wattr[(int)JOB_ATR_total_resources]);

  /* Step (2.1)
   * - expand the nodespec
   *   - add global parts to each local part
   */
  expand_nodespec(spec);

  /* Step (2.2)
   * - translate scratch resource into scratch_volume and scratch_type
   */
  if ((pjob->ji_wattr[(int)JOB_ATR_resource].at_flags & ATR_VFLAG_SET) != 0)
    {
  	if ((rd = find_resc_def(svr_resc_def,"scratch",svr_resc_size)) != 0)
	    {
      rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],rd);
      if (rs != NULL)
        {
        add_scratch_to_nodespec(spec,rs->rs_value.at_val.at_str);
        }
	    }
    }

  /* Step (3)
   * - go through normal resources
   */

  if ((pjob->ji_wattr[(int)JOB_ATR_resource].at_flags & ATR_VFLAG_SET) != 0)
    {
    resource *jbrc = (resource *)GET_NEXT(pjob->ji_wattr[(int)JOB_ATR_resource].at_val.at_list);
    while (jbrc != NULL)
      {
      rd = jbrc->rs_defin;
      if ((rd->rs_flags & ATR_DFLAG_SELECT_MOM) || (rd->rs_flags & ATR_DFLAG_SELECT_PROC)) /* per proc or per node resource */
        { /* always add to nodespec */
        tlist_head head;
        svrattrl *patlist;
        CLEAR_HEAD(head);
        rd->rs_encode(&jbrc->rs_value,&head,"ignored",rd->rs_name,ATR_ENCODE_CLIENT);
        patlist = (svrattrl *)GET_NEXT(head);
        add_res_to_nodespec(spec,patlist->al_atopl.resource,patlist->al_atopl.value);
        free_attrlist(&head);
        }
      else
        { /* per job resource, if counted, add to total resources */
        rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
        switch (rd->rs_type)
          {
          case ATR_TYPE_LONG: case ATR_TYPE_LL: case ATR_TYPE_SHORT: case ATR_TYPE_SIZE:
            {
            if (rs == NULL)
              {
              rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
              ret = rd->rs_set(&rs->rs_value,&jbrc->rs_value,SET);
              if (ret != 0)
                goto done;
              }
            else
              {
              ret = jbrc->rs_defin->rs_set(&rs->rs_value,&jbrc->rs_value,INCR);
              if (ret != 0)
                goto done;
              }
            break;
            }
          case ATR_TYPE_STR:
            {
            if (strstr(jbrc->rs_defin->rs_name,"nodes") == NULL)
              {
              if (rs == NULL)
                {
                rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
                ret = jbrc->rs_defin->rs_set(&rs->rs_value,&jbrc->rs_value,SET);
                if (ret != 0)
                  goto done;
                }
              else
                {
                ret = jbrc->rs_defin->rs_set(&rs->rs_value,&jbrc->rs_value,SET);
                if (ret != 0)
                  goto done;
                }
              }
            break;
            }
          default: break;
          }
        }
      jbrc = (resource*)GET_NEXT(jbrc->rs_link);
      }
    }

  /* Step (4)
   * - construct the processed_nodes resource
   */
  if ((rd = find_resc_def(svr_resc_def,"processed_nodes",svr_resc_size)) != NULL)
    {
    if ((rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],rd)) != NULL)
      {
      char *buf = concat_nodespec(spec,1,without_alternative,NULL);
      rd->rs_free(&rs->rs_value);
      rd->rs_decode(&rs->rs_value,NULL,NULL,buf);
      free(buf);
      }
    else
      {
      if ((rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],rd)) != NULL)
        {
        char *buf = concat_nodespec(spec,1,without_alternative,NULL);
        rd->rs_decode(&rs->rs_value,NULL,NULL,buf);
        free(buf);
        }
      }
    }

  /* Step (5)
   * - go through the parsed spec and add counted parts into the total resources
   */

  node = spec->nodes;
  while (node != NULL)
    {
    prop = node->properties;
    while (prop != NULL)
      {
      if (prop->value != NULL && (rd = find_resc_def(svr_resc_def,prop->name,svr_resc_size)) != NULL)
        {
        resource decoded;
        int total_count = 1;

        // blacklist
        if (strcmp(prop->name,"minspec")==0 ||
            strcmp(prop->name,"maxspec")==0)
          {
          prop = prop->next;
          continue;
          }

        if ((rd->rs_flags & ATR_DFLAG_SELECT_MOM) != 0)
          total_count = node->node_count;
        if ((rd->rs_flags & ATR_DFLAG_SELECT_PROC) != 0)
          {
          prop = prop->next;
          continue;
          }

        switch (rd->rs_type)
          {
          case ATR_TYPE_LONG: case ATR_TYPE_LL: case ATR_TYPE_SHORT: case ATR_TYPE_SIZE:
            break;
          default: total_count = 0; break;
          }

        if (total_count == 0)
          {
          prop = prop->next;
          continue;
          }

        ret = rd->rs_decode(&decoded.rs_value,0,prop->name,prop->value);
        if (ret != 0)
          goto done;

        rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);

        for (i = 0;i < total_count;i++)
          {
          if (rs == NULL)
            {
            rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
            ret = rd->rs_set(&rs->rs_value,&decoded.rs_value,SET);
            if (ret != 0)
              goto done;
            continue;
            }

          ret = rd->rs_set(&rs->rs_value,&decoded.rs_value,INCR);
          if (ret != 0)
            goto done;
          }
        }
      prop = prop->next;
      }
    node = node->next;
    }

  /* Step (6)
   * - add procs and nodect
   */
    {
    char buf[128];
    attribute attr;

    /* find the definition and decode the value */
    rd = find_resc_def(svr_resc_def, "procs", svr_resc_size);
    sprintf(buf,"%d",spec->total_procs);
    rd->rs_decode(&attr,"procs","",buf);

    /* check if procs are present */
    rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
    if (rs == NULL)
      rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);

    ret = rd->rs_set(&rs->rs_value,&attr,SET);
    if (ret != 0)
      goto done;

    /* find the definition and decode the value */
    rd = find_resc_def(svr_resc_def, "nodect", svr_resc_size);
    sprintf(buf,"%d",spec->total_nodes);
    rd->rs_decode(&attr,"nodect","",buf);

    /* check if procs are present */
    rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
    if (rs == NULL)
      rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);

    ret = rd->rs_set(&rs->rs_value,&attr,SET);
    if (ret != 0)
      goto done;
    }

  /* Step (7)
   * - add mem and vmem and scratch
   */
  node = spec->nodes;
  while (node != NULL)
    {
    mem += node->node_count * node->mem;
    vmem += node->node_count * node->vmem;
    scratch += node->node_count * node->scratch;
    node = node->next;
    }

    {
    char buf[128];
    attribute attr;

    if (mem != 0)
      {
      /* find the definition and decode the value */
      rd = find_resc_def(svr_resc_def, "mem", svr_resc_size);
      sprintf(buf,"%lluKB",mem);
      rd->rs_decode(&attr,"mem","",buf);

      /* check if mem is already present (shouldn't be) */
      rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
      if (rs == NULL)
        rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);

      ret = rd->rs_set(&rs->rs_value,&attr,SET);
      if (ret != 0)
        goto done;
      }

    if (vmem != 0)
      {
      /* find the definition and decode the value */
      rd = find_resc_def(svr_resc_def, "vmem", svr_resc_size);
      sprintf(buf,"%lluKB",vmem);
      rd->rs_decode(&attr,"vmem","",buf);

      /* check if vmem is already present (shouldn't be) */
      rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
      if (rs == NULL)
        rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);

      ret = rd->rs_set(&rs->rs_value,&attr,SET);
      if (ret != 0)
        goto done;
      }

    if (scratch != 0)
      {
      /* find the definition and decode the value */
      rd = find_resc_def(svr_resc_def, "scratch_volume", svr_resc_size);
      sprintf(buf,"%lluKB",scratch);
      rd->rs_decode(&attr,"scratch_volume","",buf);

      /* check if scratch is already present (shouldn't be) */
      rs = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);
      if (rs == NULL)
      rs = add_resource_entry(&pjob->ji_wattr[(int)JOB_ATR_total_resources],rd);

      ret = rd->rs_set(&rs->rs_value,&attr,SET);
      if (ret != 0)
        goto done;
      }
    }

done:
  free_parsed_nodespec(spec);

  return;
  }
