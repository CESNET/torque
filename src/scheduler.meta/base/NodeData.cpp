/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#include "NodeData.h"
#include "MiscHelpers.h"
#include "pbs_ifl.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
using namespace std;

namespace Scheduler {
namespace Base {

NodeData::NodeData() : p_phys_props(), p_virt_props(), p_priority(0), p_type(NodeCluster), p_no_multinode(false), p_ded_queue_name(NULL) {}
NodeData::~NodeData()
  {
  free(p_ded_queue_name);
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

bool NodeData::has_phys_prop(const char *name) const
  {
  return (this->p_phys_props.find(string(name)) != this->p_phys_props.end());
  }

bool NodeData::has_virt_prop(const char *name) const
  {
  return (this->p_virt_props.find(string(name)) != this->p_virt_props.end());
  }

}}
