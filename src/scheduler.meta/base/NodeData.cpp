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

namespace Scheduler {
namespace Base {

NodeData::NodeData() :  p_phys_props(), p_virt_props(), p_priority(0), p_type(NodeCluster),
                        p_no_multinode(false), p_ded_queue_name(NULL), p_admin_slot_enabled(false),
                        p_admin_slot_avail(false), p_node_cost(1.0), p_node_spec(10.0),
                        p_avail_before(0), p_avail_after(0), p_resc(NULL), p_reg_props(100,numeric_limits<size_t>::max()) {}

NodeData::~NodeData()
  {
  free(p_ded_queue_name);
  free_resource_list(p_resc);
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

void NodeData::set_ded_queue(char *data)
  {
  this->p_ded_queue_name = strdup(data);
  }

void NodeData::set_admin_slot_enabled(const char *value)
  {
  this->p_admin_slot_enabled = (strcmp(value,"True") == 0);
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

void NodeData::set_resource_capacity(const char *name, char *value)
  {
  resource *resc = find_alloc_resource(this->p_resc, name);
  if (resc == NULL)
    throw runtime_error("Couldn't allocate a new resource.");

  if (this->p_resc == NULL)
    this->p_resc = resc; // TODO ugly, needs rewrite -- find_alloc_resource needs change

  if (is_num(value))
    {
    resc->is_string = 0;
    resc->max = res_to_num(value);
    resc->str_avail = strdup(value);
    }
  else
    {
    resc->is_string = 1;
    resc->max = UNSPECIFIED;
    resc->str_avail = strdup(value);
    resc->avail = UNSPECIFIED;
    resc->assigned = UNSPECIFIED;

    pair<size_t,size_t> ids = get_prop_registry()->register_property(name,value);
    this->add_reg_props(ids.first,ids.second);
    }
  }

void NodeData::set_resource_utilisation(const char *name, char *value)
  {
  resource *resc = find_alloc_resource(this->p_resc, name);
  if (resc == NULL)
    throw runtime_error("Couldn't allocate a new resource.");

  if (this->p_resc == NULL)
    this->p_resc = resc; // TODO ugly, needs rewrite -- find_alloc_resource needs change

  resc->assigned = res_to_num(value);
  if (resc->max == UNSPECIFIED)
    resc->max = 0;
  }

void NodeData::set_resource_dynamic(const char *name, char *value)
  {
  resource *resc = find_alloc_resource(this->p_resc, name);
  if (resc == NULL)
    throw runtime_error("Couldn't allocate a new resource.");

  if (this->p_resc == NULL)
    this->p_resc = resc; // TODO ugly, needs rewrite -- find_alloc_resource needs change

  free(resc->str_avail);

  if (is_num(value))
    {
    resc->is_string = 0;
    resc->avail = res_to_num(value);
    resc->str_avail = strdup(value);
    resc->assigned = 0;
    resc->max = UNSPECIFIED; // dynamic resources don't have capacity
    }
  else
    {
    resc->is_string = 1;
    resc->avail = 0;
    resc->str_avail = strdup(value);
    resc->assigned = 0;
    resc->max = UNSPECIFIED;

    pair<size_t,size_t> ids = get_prop_registry()->register_property(name,value);
    this->add_reg_props(ids.first,ids.second);
    }
  }

resource *NodeData::get_resource(const char *name) const
  {
  return find_resource(this->p_resc,name);
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
    p_reg_props.resize(oldsize+max(size_t(50),propid-oldsize),numeric_limits<size_t>::max());
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

}}
