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
extern char *start_sbf_vlan(char *clusterid, char *nodelist);
extern int stop_sbf_vlan(char *vlanid);
extern void free_prop_list(struct prop*);
extern struct prop  *init_prop(char *pname);

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

int is_cloud_job_private(job *pjob)
  {
  resource *pres = find_resc_entry(&pjob->ji_wattr[(int)JOB_ATR_resource],
		   find_resc_def(svr_resc_def,"net",svr_resc_size));

  if (pres != NULL && (pres->rs_value.at_flags & ATR_VFLAG_SET) &&
      strcmp(pres->rs_value.at_val.at_str,"private") == 0)
    return 1;
  
  return 0;
  }

static char *construct_mapping(char* old, char* new, char* alternative)
  {
  /* cloud=virtual[alternative]; */
  char *result = malloc(strlen(old)+1+strlen(new)+3+strlen(alternative)+1);
  sprintf(result,"%s=%s[%s];",old,new,alternative);
  return result;
  }

struct pars_prop *get_last_prop(struct pars_prop *i)
  {
  while (i->next != NULL)
    i = i->next;

  return i;
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

  char mapping[1024] = { 0 }; /* FIXME META Needs a dynamic array */

  if ((ps = parse_nodespec(nodespec)) == NULL)
    return NULL;

  iter = ps->nodes;

  while (iter != NULL) /* for each node */
    {
    char *ret; /* FIXME META ported from original PATCH, needs checking */

    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");

    ret=pbs_cache_get_local(get_last_prop(iter->properties)->name,"host");
    if (ret!=NULL)
      {
      char *c, *cloud, *mapped;
      pars_prop *it2;

      /* cut out the cloud node name from the cache value */
      c=strchr(ret,'\t');
      cloud = strdup(++c);
      c=strchr(cloud,'\n');
      if (c)
        *c = '\0';
      free(ret);

      it2 = iter->properties;
      while (it2 != NULL)
        {
        if ((strcmp(it2->name,"alternative") == 0) && (it2->value != NULL))
          break;

        it2 = it2->next;
        }

      if (it2 != NULL)
        mapped = construct_mapping(cloud,iter->properties->name,it2->value);
      else
        mapped = construct_mapping(cloud,iter->properties->name,"default");
      strcat(mapping,mapped);
      free(mapped);

      /* interchange virtual node name for its cloud master */
      free(iter->properties->name);
      iter->properties->name=cloud;
      }
    iter = iter->next;
    }

  /* store the constructed mapping into job attribute */
  job_attr_def[(int)JOB_ATR_cloud_mapping].at_decode(&pjob->ji_wattr[(int)JOB_ATR_cloud_mapping],
                                                      (char *)0, (char *)0, mapping);

  sprintf(log_buffer,"Source: %s Target: %s",nodespec,concat_nodespec(ps));
  log_record(PBSEVENT_SCHED,PBS_EVENTCLASS_REQUEST,"switch_nodespec_to_cloud",log_buffer);
  return concat_nodespec(ps); /* FIXME META needs fortification and fix of memory leak */
  }

/* FIXME META DUPLICATED WITH SITE PBS CACHE */
#define PBS_REPLICA_IMAGES   "/var/local/repository/images"
extern int check_and_read_config(char *filename,config_line_s **lines,time_t *last_modification,int *last_line);
static config_line_s *replica_images_lines[10240]; /* FIX ME - dynamicaly allocated? */
static time_t replica_images_last_modification=0;
static int replica_images_last_line=-1;

char *get_alternative_properties(char *name)
{ int ret;
  int i;

  ret=check_and_read_config(PBS_REPLICA_IMAGES,replica_images_lines,&replica_images_last_modification,&replica_images_last_line);
  if (ret==-1) return NULL;

  for(i=0;replica_images_lines[i];i++)
      if (strcmp(replica_images_lines[i]->key,name)==0)
    return(replica_images_lines[i]->first);
  return NULL;
}

void set_alternative_on_node(char *nodename, char *alternative)
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

  c = get_alternative_properties(alternative);

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

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_NODE, np->nd_name, "additional properties cleared from node");
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


void cloud_transition_into_prerun(job *pjob)
  {
  char *vlanid;

  svr_setjobstate(pjob,JOB_STATE_RUNNING,JOB_SUBSTATE_PRERUN_CLOUD);
  if (is_cloud_job_private(pjob))
    {
    vlanid=start_sbf_vlan(pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str,
                          pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str);
    if (vlanid !=NULL)
      {
      job_attr_def[(int)JOB_ATR_vlan_id].at_decode(&pjob->ji_wattr[(int)JOB_ATR_vlan_id],
                                                  (char *)0, (char *)0, vlanid);

      }
    }
  }

void cloud_transition_into_running(job *pjob)
  {
  pars_spec *ps;
  pars_spec_node *iter;

  if (is_cloud_job(pjob) && pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN_CLOUD)
    svr_setjobstate(pjob, JOB_STATE_RUNNING, JOB_SUBSTATE_RUNNING);

  dbg_consistency(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_flags & ATR_VFLAG_SET,
      "JOB_ATR_sched_spec has to be set at this point");

  ps = parse_nodespec(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_val.at_str);
  dbg_consistency(ps != NULL, "The nodespec should be well formed when reaching this point.");
  if (ps == NULL)
    return;

  iter = ps->nodes;

  while (iter != NULL)
    {
    char *c;

    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");
    if (iter->properties == NULL)
      { iter = iter->next; continue; }

    /* update cache information that this machine now belongs to the following vcluster */
    cache_store_local(iter->properties->name,"machine_cluster",pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str);

    /* dig up the alternative from cloud mapping */
    dbg_consistency((pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_flags & ATR_VFLAG_SET) != 0,
        "Cloud mapping has to be set at this point.");

    c = get_alternative_name(pjob->ji_wattr[(int)JOB_ATR_cloud_mapping].at_val.at_str,iter->properties->name);
    if (c != NULL)
      {
      sprintf(log_buffer,"Determined alternative is: %s", c);
      log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, iter->properties->name,log_buffer);
      set_alternative_on_node(iter->properties->name,c);
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

  dbg_consistency(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_flags & ATR_VFLAG_SET,
      "JOB_ATR_sched_spec has to be set at this point");

  ps = parse_nodespec(pjob->ji_wattr[(int)JOB_ATR_sched_spec].at_val.at_str);
  dbg_consistency(ps != NULL, "The nodespec should be well formed when reaching this point.");
  if (ps == NULL)
    return;

  iter = ps->nodes;

  while (iter != NULL)
    {
    /* there has to be at least node name */
    dbg_consistency(iter->properties != NULL, "Wrong nodespec format.");
    if (iter->properties == NULL)
      { iter = iter->next; continue; }

    /* update cache information that this machine now belongs to the following vcluster */
    cache_remove_local(iter->properties->name,"machine_cluster");

    /* remove any alternative stored on the node */
    clear_alternative_on_node(iter->properties->name);

    iter = iter->next;
    }

  free_parsed_nodespec(ps);

  if (is_cloud_job_private(pjob))
    {
    if (pjob->ji_wattr[(int)JOB_ATR_vlan_id].at_val.at_str != NULL)
      stop_sbf_vlan(pjob->ji_wattr[(int)JOB_ATR_vlan_id].at_val.at_str);
    }
  }



#define SBF_START "/var/spool/torque/server_priv/sbf_start"
#define SBF_STOP "/var/spool/torque/server_priv/sbf_stop"

/* start_sbf_vlan
 * start SBF vlan, pass list of hosts to SBF, read vlanid back
 */
char *start_sbf_vlan(char *clusterid, char *nodelist)
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
int stop_sbf_vlan(char *vlanid)
{ FILE *fp;
  char buf[256];
  int ret=0;
  char execbuf[1024];

  strcpy(execbuf,SBF_STOP);
  strcat(execbuf," ");
  strcat(execbuf,vlanid);

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

