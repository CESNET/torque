#include "cloud.h"
#include "resource.h"
#include "assertions.h"
#include "nodespec.h"
#include "api.h"
#include "pbs_job.h"
#include "pbs_nodes.h"
#include "site_pbs_cache.h"

#include <string.h>


extern struct pbsnode *find_nodebyname(char *);
extern char *start_sbf_vlan(char *clusterid, char *nodelist, char *netresc);
extern int stop_sbf_vlan(char *vlanid, char *nodelist, char *netresc);
extern void free_prop_list(struct prop*);
extern struct prop  *init_prop(char *pname);

/** Return the node name from the properties list (last property) */
static pars_prop *get_name_prop(pars_spec_node *node)
  {
  pars_prop *iter = node->properties;
  return iter;
  }

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

/** Determine the type of private network usage
 * 
 * 0 - no VPN
 * 1 - new VPN
 * 2 - id provided - attach to existing VPN
 */
int is_cloud_job_private(job *pjob, char** netresc)
  {
  resource *pres = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],
		   find_resc_def(svr_resc_def,"net",svr_resc_size));

  if (pres != NULL && (pres->rs_value.at_flags & ATR_VFLAG_SET))
    {
    *netresc = pres->rs_value.at_val.at_str;

    if (strcmp(pres->rs_value.at_val.at_str,"private") == 0)
      return 1;
    else
      return 2;
    }
  
  return 0;
  }

static char *construct_mapping(char* old, char* new, char* alternative)
  {
  /* cloud=virtual[alternative]; */
  char *result = malloc(strlen(old)+1+strlen(new)+3+strlen(alternative)+1);
  sprintf(result,"%s=%s[%s];",old,new,alternative);
  return result;
  }

/* switch virtual nodes in the nodespec to their cloud masters
 * only used for cloud jobs support
 */
char *switch_nodespec_to_cloud(job  *pjob, char *nodespec)
  {
  /* The nodespec is expected in the following format:
   * node1:res1=val1:res2=val2+node2:res3=val3...
   */
  pars_spec * ps;
  pars_spec_node *iter;
  pars_prop * prop;

  char mapping[1024] = { 0 }; /* FIXME META Needs a dynamic array */

  if ((ps = parse_nodespec(nodespec)) == NULL)
    return NULL;

  iter = ps->nodes;

  while (iter != NULL) /* for each node */
    {
    char *ret; /* FIXME META ported from original PATCH, needs checking */

    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");

    prop = get_name_prop(iter);

    ret=pbs_cache_get_local(prop->name,"host");
    if (ret!=NULL)
      {
      char *c, *cloud, *mapped;

      /* cut out the cloud node name from the cache value */
      c=strchr(ret,'\t');
      cloud = strdup(++c);
      c=strchr(cloud,'\n');
      if (c)
        *c = '\0';
      free(ret);

      if (iter->alternative != NULL)
        mapped = construct_mapping(cloud,prop->name,iter->alternative);
      else
        mapped = construct_mapping(cloud,prop->name,"default");
      strcat(mapping,mapped);
      free(mapped);

      /* interchange virtual node name for its cloud master */
      free(prop->name);
      prop->name=cloud;
      }
    iter = iter->next;
    }

  /* store the constructed mapping into job attribute */
  job_attr_def[(int)JOB_ATR_cloud_mapping].at_decode(&pjob->ji_wattr[(int)JOB_ATR_cloud_mapping],
                                                      (char *)0, (char *)0, mapping);

  sprintf(log_buffer,"Source: %s Target: %s",nodespec,concat_nodespec(ps,1,NULL));
  log_record(PBSEVENT_SCHED,PBS_EVENTCLASS_REQUEST,"switch_nodespec_to_cloud",log_buffer);
  return concat_nodespec(ps,1,NULL); /* FIXME META needs fortification and fix of memory leak */
  }

extern int check_and_read_config(char *filename,config_line_s **lines,time_t *last_modification,int *last_line);

void set_alternative_on_node(char *nodename, char *alternative, char *cloud_name)
  {
  char            *c;
  struct pbsnode  *np;
  struct attribute props;
  struct prop    **plink;
  struct prop     *pdest;
  int              nad_props;
  int              i;
  extern attribute_def node_attr_def[];

  np = find_nodebyname(nodename);
  if ((np = find_nodebyname(nodename)) == NULL)
    return;

  log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, nodename, "Found node and setting alternative.");

  c = get_alternative_properties(alternative);

  if (c == NULL)
    return;

  log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, nodename, "Found alternative and setting.");

  /* clean the previous data */
  free_prop_list(np->x_ad_prop);
  if (np->x_ad_properties)
    free(np->x_ad_properties->as_buf);
  free(np->x_ad_properties);

  np->xn_ad_prop = 0;
  np->x_ad_properties = NULL;
  np->x_ad_prop = NULL;

  props.at_val.at_arst = 0;
  props.at_flags = 0;
  props.at_type = ATR_TYPE_ARST;

  /* decode the new value into the temporary attribute */
  node_attr_def[(int)ND_ATR_adproperties].at_decode(&props,
                                                    ATTR_NODE_adproperties,
                                                    0,
                                                    c);
  /* set the real structure attribute */
  node_attr_def[(int)ND_ATR_adproperties].at_action(&props,
                                                    np,
                                                    ATR_ACTION_ALTER);

  /* update additional properties list based on new additional properties array */
  plink = &np->x_ad_prop;

  if (np->x_ad_properties != NULL)
    {
    nad_props = np->x_ad_properties->as_usedptr;

    for (i = 0; i < nad_props; ++i)
      {
      pdest = init_prop(np->x_ad_properties->as_string[i]);

      *plink = pdest;
      plink = &pdest->next;
      }
    }

  np->xn_ad_prop = nad_props;

  free(np->cloud);
  np->cloud = strdup(cloud_name);

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_NODE, np->nd_name, "additional properties set on node");
  }

void clear_alternative_on_node(char *nodename)
  {
  struct pbsnode *np;

  if ((np = find_nodebyname(nodename)) == NULL)
    return;

  free_prop_list(np->x_ad_prop);
  if (np->x_ad_properties)
    free(np->x_ad_properties->as_buf);
  free(np->x_ad_properties);
  np->x_ad_prop = NULL;
  np->x_ad_properties = NULL;
  np->xn_ad_prop = 0;

  free(np->cloud);
  np->cloud = NULL;

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_NODE, np->nd_name, "additional properties cleared from node");
  }

void reset_alternative_on_node(job *pjob)
  {
  if (is_cloud_job(pjob))
    {
    if ((pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_flags & ATR_VFLAG_SET) != 0)
      {
      char *map, *p;

      p = map = strdup(pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_val.at_str);
      do
        {
        char *host, *alternative, *c;

        c = strchr(p,';');
        if (c != NULL)
          {
          *c = '\0';
          c++;
          }

        host = strchr(p,'=');
        if (host == NULL)
          return;

        host++;

        alternative = strchr(p,'[');
        if (alternative == NULL)
          return;

        *alternative = '\0';
        alternative++;
        alternative[strlen(alternative)-1] = '\0';

        set_alternative_on_node(host,alternative,pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str);

        p = c;
        }
      while (p != NULL);

      free(map);
      }
    }
  }

char *get_alternative_name(char *mapping, char *machine)
  {
  char *mpos, *begin, *end, *result;

  if ((mpos = strstr(mapping,machine)) == NULL)
    return NULL;

  if ((begin = strchr(mpos,'[')) == NULL)
    return NULL;

  begin++;

  if ((end = strchr(mpos,']')) == NULL)
    return NULL;

  end--;

  result = malloc(end-begin+2);
  memset(result,0,end-begin+2);
  strncpy(result,begin,end-begin+1);

  return result;
  }


int cloud_transition_into_prerun(job *pjob)
  {
  char     *vlanid = NULL;
  char     *cached = NULL;
  resource *pres = NULL;
  char     *tmp = NULL, *owner = NULL, *jowner = NULL;
  char     *netresc = NULL;

  svr_setjobstate(pjob,JOB_STATE_RUNNING,JOB_SUBSTATE_PRERUN_CLOUD);
  if (is_cloud_job_private(pjob,&netresc))
    {
    vlanid=start_sbf_vlan(pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str,
                          pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,
			  netresc);
    if (vlanid !=NULL)
      {
      job_attr_def[(int)JOB_ATR_vlan_id].at_decode(&pjob->ji_wattr[(int)JOB_ATR_vlan_id],
                                                  (char *)0, (char *)0, vlanid);

      }
    else
      {
      return 1;
      }
    }

  /* udpate cache information */
  jowner = pjob->ji_wattr[(int)JOB_ATR_job_owner].at_val.at_str;

  tmp = strchr(jowner,'@');
  if (tmp == NULL)
    {
    owner = strdup(jowner);
    }
  else
    {
    owner = malloc(tmp - jowner + 1);
    strncpy(owner,jowner,tmp - jowner);
    owner[tmp - jowner] = '\0';
    }

  store_cluster_attr(&cached,"owner",owner);
  free(owner);

  if (vlanid)
    {
    store_cluster_attr(&cached,"vlan",vlanid);
    free(vlanid);
    }

  pres = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],
                          find_resc_def(svr_resc_def, "group", svr_resc_size));

  if (pres != NULL && ((pres->rs_value.at_flags & ATR_VFLAG_SET) != 0))
    {
    store_cluster_attr(&cached, "group", pres->rs_value.at_val.at_str);
    }

  cache_store_local(pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str, "cluster", cached);

  free(cached);

  return 0;
  }

void cloud_transition_into_running(job *pjob)
  {
  pars_spec *ps;
  pars_spec_node *iter;
  char *cloud_name;

  if (is_cloud_job(pjob) && pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN_CLOUD)
    svr_setjobstate(pjob, JOB_STATE_RUNNING, JOB_SUBSTATE_RUNNING);

  dbg_consistency(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_flags & ATR_VFLAG_SET,
      "JOB_ATR_sched_spec has to be set at this point");

  ps = parse_nodespec(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_val.at_str);
  dbg_consistency(ps != NULL, "The nodespec should be well formed when reaching this point.");
  if (ps == NULL)
    return;

  cloud_name = pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str;

  iter = ps->nodes;

  while (iter != NULL)
    {
    char *c;
    pars_prop *name;

    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");
    if (iter->properties == NULL)
      { iter = iter->next; continue; }

    name = get_name_prop(iter);

    /* update cache information that this machine now belongs to the following vcluster */
    cache_store_local(name->name,"machine_cluster",pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str);

    /* dig up the alternative from cloud mapping */
    dbg_consistency((pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_flags & ATR_VFLAG_SET) != 0,
        "Cloud mapping has to be set at this point.");

    c = get_alternative_name(pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_val.at_str,name->name);
    if (c != NULL)
      {
      sprintf(log_buffer,"Determined alternative is: %s", c);
      log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, name->name,log_buffer);
      set_alternative_on_node(name->name,c,cloud_name);
      free(c);
      }

    iter = iter->next;
    }
  free_parsed_nodespec(ps);
  }

void cloud_transition_into_stopped(job *pjob)
  {
  pars_spec *ps;
  pars_spec_node *iter;
  char *netresc = NULL;

  dbg_consistency(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_flags & ATR_VFLAG_SET,
      "JOB_ATR_sched_spec has to be set at this point");

  ps = parse_nodespec(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_val.at_str);
  dbg_consistency(ps != NULL, "The nodespec should be well formed when reaching this point.");
  if (ps == NULL)
    return;

  iter = ps->nodes;

  while (iter != NULL)
    {
    pars_prop *name;

    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");
    if (iter->properties == NULL)
      { iter = iter->next; continue; }

    name = get_name_prop(iter);

    /* update cache information that this machine now belongs to the following vcluster */
    cache_remove_local(name->name,"machine_cluster");

    /* remove any alternative stored on the node */
    clear_alternative_on_node(name->name);

    iter = iter->next;
    }

  free_parsed_nodespec(ps);

  if (is_cloud_job_private(pjob,&netresc))
    {
    if (pjob->ji_wattr[(int)JOB_ATR_vlan_id].at_val.at_str != NULL)
      stop_sbf_vlan(pjob->ji_wattr[(int)JOB_ATR_vlan_id].at_val.at_str,
		    pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,
		    netresc);
    }

  cache_remove_local(pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str,"cluster");
  }

#define SBF_START "/var/spool/torque/server_priv/sbf_start"
#define SBF_STOP "/var/spool/torque/server_priv/sbf_stop"

/* start_sbf_vlan
 * start SBF vlan, pass list of hosts to SBF, read vlanid back
 */
char *start_sbf_vlan(char *clusterid, char *nodelist, char *netresc)
{ FILE *fp;
  char buf[256];
  char execbuf[10024]; /* TODO FIX */
  char *c,*s,*ss,*cc;
  char delim[2];

  ss=s=strdup(nodelist);
  delim[0]='+';
  delim[1]='\0';

  strcpy(execbuf,SBF_START);
  strcat(execbuf," ");

  if (netresc != NULL && strcmp(netresc,"private") != 0)
    {
    strcat(execbuf,"-v ");
    strcat(execbuf,netresc);
    strcat(execbuf," ");
    }

  strcat(execbuf,clusterid);
  strcat(execbuf," ");

  while (((c=strsep(&s,delim))!=NULL) ) {
        cc=strchr(c,'/');
        if (cc!=NULL)
          *cc='\0';
        cc=strchr(c,':');
        if (cc!=NULL)
          *cc='\0';
        cc=strchr(c,'[');
        if (cc!=NULL)
          *cc='\0';

        strcat(execbuf," ");
  strcat(execbuf,c);
  }
  free(ss);

  if( (fp = popen(execbuf,"r")) != NULL ) {
      if( fgets(buf, 256, fp) != NULL ) {
    if (strlen(buf)<=1) {
              log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,"start_sbf_vlan","error in read");
        return NULL;
    }
    buf[strlen(buf)-1] = '\0';
      } else {
          log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,"start_sbf_vlan","error in fgets");
    return NULL;
      }
      pclose(fp);
  } else {
      log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,"start_sbf_vlan","error in popen");
      return NULL;
  }

  return strdup(buf);
}

/* stop_sbf_vlan
 * stop SBF vlan created by start_sbf_vlan()
 */
int stop_sbf_vlan(char *vlanid, char *nodelist, char *netresc)
{ FILE *fp;
  char buf[256];
  int ret=0;
  char execbuf[1024];
  char *c,*s,*ss,*cc;
  char delim[2];

  ss=s=strdup(nodelist);
  delim[0]='+';
  delim[1]='\0';

  strcpy(execbuf,SBF_STOP);
  strcat(execbuf," ");

  if (netresc != NULL && strcmp(netresc,"private") != 0)
    {
    strcat(execbuf,"-k ");
    }

  strcat(execbuf,vlanid);
  strcat(execbuf," ");

  while (((c=strsep(&s,delim))!=NULL) ) {
        cc=strchr(c,'/');
        if (cc!=NULL)
          *cc='\0';
        cc=strchr(c,':');
        if (cc!=NULL)
          *cc='\0';
        cc=strchr(c,'[');
        if (cc!=NULL)
          *cc='\0';

        strcat(execbuf," ");
  strcat(execbuf,c);
  }
  free(ss);

  if( (fp = popen(execbuf,"r")) != NULL ) {
      if( fgets(buf, 256, fp) != NULL ) {
    buf[strlen(buf)-1] = '\0';
    if (strcmp(buf,"ok")!=0) {
              log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,"stop_sbf_vlan","error in read");
        ret=-1;
    }
      } else {
          log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,"stop_sbf_vlan","error in fgets");
    ret=-1;
      }
      pclose(fp);
  } else {
      log_record(PBSEVENT_SYSTEM,PBS_EVENTCLASS_SERVER,"stop_sbf_vlan","error in popen");
      ret=-1;
  }

  return ret;
}

