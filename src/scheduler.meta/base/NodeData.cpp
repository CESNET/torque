/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#include "NodeData.h"
#include "MiscHelpers.h"
extern "C" {
  #include "pbs_ifl.h"
}
#include "server_info.h" // TODO move functions to base
#include "misc.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <limits>
#include "base/PropRegistry.h"
using namespace std;
using namespace boost;

namespace Scheduler {
namespace Base {

NodeData::NodeData(struct batch_status *node_data) :  p_phys_props(), p_virt_props(), p_priority(0), p_type(NodeCluster),
                        p_no_multinode(false), p_ded_queue_name(), p_admin_slot_enabled(false),
                        p_admin_slot_avail(false), p_node_cost(1.0), p_node_spec(10.0),
                        p_avail_before(0), p_avail_after(0), p_resc(), p_reg_props(100,numeric_limits<size_t>::max()), p_name(), p_jobs(),
                        p_exclusively_assigned(false), p_scratch_pool(), p_server(NULL)
  {
  struct attrl *attrp = node_data->attribs;  /* used to cycle though attribute list */
  this->set_name(node_data->name);

  while (attrp != NULL)
    {
    /* properties from the servers nodes file */
    if (!strcmp(attrp -> name, ATTR_NODE_properties))
      this->set_phys_props(attrp->value);
    /* properties from the servers nodes file */
    else if (!strcmp(attrp -> name, ATTR_NODE_adproperties))
      this->set_virt_props(attrp->value);
    /* Node State... i.e. offline down free etc */
    else if (!strcmp(attrp -> name, ATTR_NODE_state))
      this->reset_state(attrp -> value);
    /* Node priority */
    else if (!strcmp(attrp -> name, ATTR_NODE_priority))
      this->set_priority(attrp->value);
    /* the node type... i.e. timesharing or cluster */
    else if (!strcmp(attrp -> name, ATTR_NODE_ntype))
      this->set_type(attrp->value);
    /* No multinode jobs on node */
    else if (!strcmp(attrp -> name, ATTR_NODE_no_multinode_jobs))
      this->set_nomultinode(attrp->value);
    /* No starving reservations on this node */
    else if (!strcmp(attrp -> name, ATTR_NODE_noautoresv))
      this->set_nostarving(attrp->value);
    /* Total number of cores on node */
    else if (!strcmp(attrp -> name, ATTR_NODE_np))
      this->set_proc_total(attrp->value);
    /* Free cores on node */
    else if (!strcmp(attrp -> name, ATTR_NODE_npfree))
      this->set_proc_free(attrp->value);
    /* Name of the dedicated queue */
    else if (!strcmp(attrp -> name, ATTR_NODE_queue))
      this->set_ded_queue(attrp->value);
    /* Is admin slot available */
    else if (!strcmp(attrp -> name, ATTR_NODE_admin_slot_available))
      this->set_admin_slot_avail(attrp->value);
    /* Is admin slot enabled */
    else if (!strcmp(attrp -> name, ATTR_NODE_admin_slot_enabled))
      this->set_admin_slot_enabled(attrp->value);
    /* Node fairshare cost */
    else if (!strcmp(attrp -> name, ATTR_NODE_fairshare_coef))
      this->set_node_cost(attrp->value);
    /* Node machine spec */
    else if (!strcmp(attrp -> name, ATTR_NODE_machine_spec))
      this->set_node_spec(attrp->value);
    /* Node available after */
    else if (!strcmp(attrp -> name, ATTR_NODE_available_after))
      this->set_avail_after(attrp->value);
    /* Node available before */
    else if (!strcmp(attrp -> name, ATTR_NODE_available_before))
      this->set_avail_before(attrp->value);
    /* Resource capacity */
    else if (!strcmp(attrp -> name, ATTR_NODE_resources_total))
      this->set_resource_capacity(attrp->resource,attrp->value);
    /* Resource utilisation */
    else if (!strcmp(attrp -> name, ATTR_NODE_resources_used))
      this->set_resource_utilisation(attrp->resource,attrp->value);
    /* the jobs running on the node */
    else if (!strcmp(attrp -> name, ATTR_NODE_jobs))
      this->set_jobs(attrp->value);
    else if (!strcmp(attrp -> name, ATTR_NODE_exclusively_assigned))
      this->set_exclusively_assigned(attrp->value);

    attrp = attrp -> next;
    }
  }

NodeData::~NodeData() {}

void NodeData::set_scratch_pool(const char *pool)
  {
  p_scratch_pool = pool;
  }

void NodeData::set_virt_props(char *comma_sep_list)
  {
  comma_list_to_set(comma_sep_list,this->p_virt_props);
  }

void NodeData::set_phys_props(char *comma_sep_list)
  {
  comma_list_to_set(comma_sep_list,this->p_phys_props);
  }

void NodeData::set_priority(char *data)
  {
  char *end = NULL;
  this->p_priority = strtol(data,&end,10);
  if (data == end)
    throw invalid_argument("Unable to convert node priority value to a number.");
  }

void NodeData::set_proc_total(char* data)
  {
  char *end = NULL;
  this->p_core_total = strtol(data,&end,10);
  if (data == end)
    throw invalid_argument("Unable to convert total cores value to a number.");
  }

void NodeData::set_proc_free(char* data)
  {
  char *end = NULL;
  this->p_core_free = strtol(data,&end,10);
  if (data == end)
    throw invalid_argument("Unable to convert free cores value to a number.");
  }

void NodeData::set_type(char *data)
  {
  if (!strcmp(data, ND_timeshared))
    this->p_type = NodeTimeshared;
  else if (!strcmp(data, ND_cluster))
    this->p_type = NodeCluster;
  else if (!strcmp(data, ND_cloud))
    this->p_type = NodeCloud;
  else if (!strcmp(data, ND_virtual))
    this->p_type = NodeVirtual;
  else
    throw invalid_argument("Unable to convert node type value.");
  }

void NodeData::set_nomultinode(char *data)
  {
  this->p_no_multinode = (strcmp(data,"True") == 0);
  }

void NodeData::set_nostarving(char *data)
  {
  this->p_no_starving = (strcmp(data,"True") == 0);
  }

void NodeData::set_ded_queue(const char *data)
  {
  this->p_ded_queue_name = data;
  }

void NodeData::set_admin_slot_enabled(const char *value)
  {
  this->p_admin_slot_enabled = (strcmp(value,"True") == 0);
  }

void NodeData::set_jobs(char *comma_sep_list)
  {
  comma_list_to_set(comma_sep_list,this->p_jobs);
  }


void NodeData::set_admin_slot_avail(const char *value)
  {
  this->p_admin_slot_avail = (strcmp(value,"True") == 0);
  }

void NodeData::set_node_cost(const char *value)
  {
  char *end = NULL;
  this->p_node_cost = strtod(value,&end);
  if (value == end)
    throw invalid_argument("Unable to convert node cost value to a number.");
  }

void NodeData::set_node_spec(const char *value)
  {
  char *end = NULL;
  this->p_node_spec = strtod(value,&end);
  if (value == end)
    throw invalid_argument("Unable to convert node spec value to a number.");
  }

void NodeData::set_avail_after(const char *data)
  {
  char *end = NULL;
  this->p_avail_after = strtol(data,&end,10);
  if (data == end)
    throw invalid_argument("Unable to convert available after value to a number.");
  }

void NodeData::set_avail_before(const char *data)
  {
  char *end = NULL;
  this->p_avail_before = strtol(data,&end,10);
  if (data == end)
    throw invalid_argument("Unable to convert available before value to a number.");
  }

void NodeData::set_exclusively_assigned(const char* value)
  {
  p_exclusively_assigned = !strcmp(value,"True");
  }

void NodeData::set_resource_capacity(const char *name, char *value)
  {
  boost::shared_ptr<Resource> result = p_resc.get_alloc_resource(string(name));
  if (!result)
    throw runtime_error("Couldn't allocate a new resource.");

  result->set_capacity(value);
  if (result->is_string())
    {
    pair<size_t,size_t> ids = get_prop_registry()->register_property(name,value);
    this->add_reg_props(ids.first,ids.second);
    }
  }

void NodeData::set_resource_utilisation(const char *name, char *value)
  {
  boost::shared_ptr<Resource> result = p_resc.get_alloc_resource(string(name));
  if (!result)
    throw runtime_error("Couldn't allocate a new resource.");

  result->set_utilization(value);
  }

void NodeData::set_resource_dynamic(const char *name, char *value)
  {
  boost::shared_ptr<Resource> result = p_resc.get_alloc_resource(string(name));
  if (!result)
    throw runtime_error("Couldn't allocate a new resource.");

  result->reset();
  result->set_avail(value);
  if (result->is_string())
    {
    pair<size_t,size_t> ids = get_prop_registry()->register_property(name,value);
    this->add_reg_props(ids.first,ids.second);
    }
  }

Resource *NodeData::get_resource(const char *name) const
  {
  boost::shared_ptr<Resource> result = p_resc.get_resource(string(name));
  if (result)
    return result.get();
  else
    return NULL;
  }

bool NodeData::has_phys_prop(const char *name) const
  {
  return (this->p_phys_props.find(string(name)) != this->p_phys_props.end());
  }

bool NodeData::has_virt_prop(const char *name) const
  {
  return (this->p_virt_props.find(string(name)) != this->p_virt_props.end());
  }

void NodeData::add_reg_props(size_t propid, size_t valueid)
  {
  if (propid >= p_reg_props.size())
    {
    size_t oldsize = p_reg_props.size();
    p_reg_props.resize(oldsize+std::max(size_t(50),propid-oldsize),numeric_limits<size_t>::max());
    }

  p_reg_props[propid] = valueid;
  }

bool NodeData::has_reg_prop(size_t propid, size_t valueid) const
  {
  if (propid >= p_reg_props.size())
    return false;

  if (p_reg_props[propid] != valueid)
    return false;

  return true;
  }

const char *NodeData::get_name() const
  {
  return p_name.c_str();
  }

void NodeData::set_name(const char *name)
  {
  p_name = name;
  }

bool NodeData::has_jobs() const
  {
  return p_jobs.size() != 0;
  }

bool NodeData::is_exclusively_assigned() const
  {
  return p_exclusively_assigned;
  }

}}
