#include "logic/JobAssign.h"
#include <sstream>
#include <cstring>
#include <cstdlib>
using namespace std;

#include "RescInfoDb.h"

namespace Scheduler {
namespace Logic {

bool JobAssign::has_fairshare_flag() const
  {
  return p_flag_fairshare;
  }

void JobAssign::unset_fairshare_flag()
  {
  p_flag_fairshare = false;
  }

void JobAssign::set_fairshare_flag()
  {
  p_flag_fairshare = true;
  }

ScratchType JobAssign::get_scratch_assign() const
  {
  return p_scratch_type;
  }

void JobAssign::set_scratch_assign(ScratchType scratch_type)
  {
  p_scratch_type = scratch_type;
  }

void JobAssign::set_selected_alternative(repository_alternatives *boot_alternative)
  {
  p_boot_alternative = boot_alternative;
  }

bool JobAssign::has_selected_alternative() const
  {
  return p_boot_alternative != NULL;
  }

repository_alternatives *JobAssign::get_selected_alternative() const
  {
  return p_boot_alternative;
  }

bool JobAssign::has_assignment() const
  {
  return p_nodespec != NULL;
  }

pars_spec_node *JobAssign::get_assignment() const
  {
  return p_nodespec;
  }

void JobAssign::set_assignment(pars_spec_node *spec)
  {
  p_nodespec = clone_pars_spec_node(spec);
  }

bool JobAssign::has_starving_assignment() const
  {
  return p_starved_nodespec != NULL;
  }

pars_spec_node *JobAssign::get_starving_assignment() const
  {
  return p_starved_nodespec;
  }

void JobAssign::set_starving_assignment(pars_spec_node *spec)
  {
  p_starved_nodespec = clone_pars_spec_node(spec);
  }

void JobAssign::clean_assign()
  {
  if (p_nodespec != NULL)
    free_pars_spec_node(&p_nodespec);
  p_nodespec = NULL;

  if (p_starved_nodespec != NULL)
    free_pars_spec_node(&p_starved_nodespec);
  p_starved_nodespec = NULL;

  p_boot_alternative = NULL;
  p_scratch_type = ScratchNone;
  }

JobAssign::JobAssign() : p_flag_fairshare(false), p_scratch_type(ScratchNone), p_boot_alternative(NULL), p_nodespec(NULL), p_starved_nodespec(NULL)
  {}

JobAssign::~JobAssign()
  {
  free_pars_spec_node(&p_nodespec);
  free_pars_spec_node(&p_starved_nodespec);
  }

void JobAssign::get_assign_string(stringstream& s, const char *node_name, AssignStringMode mode) const
  {
  s << "host=" << node_name << ":ppn=" << p_nodespec->procs;
  s << ":mem=" << p_nodespec->mem << "KB";
  s << ":vmem=" << p_nodespec->vmem << "KB";

  pars_prop *iter = p_nodespec->properties;
  while (iter != NULL)
    {
    int skip = 0;

    /* avoid duplicate hostname properties */
    if (strcmp(node_name,iter->name) == 0)
      {
      iter = iter->next;
      continue;
      }

    if (res_check_type(iter->name) == ResCheckDynamic)
      skip = 1;
    if (res_check_type(iter->name) == ResCheckCache)
      skip = 1;

    if ((!skip) && (mode == FullAssignString || /* full nodespec */
        (mode == RescOnlyAssignString && iter->value != NULL) || /* resources only */
        (mode == NumericOnlyAssignString && iter->value != NULL && atoi(iter->value) > 0))) /* integer resources only */
      {
      s << ":" << iter->name;
      if (iter->value != NULL)
        s << "=" << iter->value;
      }
    iter = iter->next;
    }

  if (p_scratch_type != ScratchNone)
    {
    s << ":scratch_type=";
    if (p_scratch_type == ScratchSSD)
      s << "ssd";
    else if (p_scratch_type == ScratchShared)
      s << "shared";
    else if (p_scratch_type == ScratchLocal)
      s << "local";
    s << ":scratch_volume=" << p_nodespec->scratch / 1024 << "mb";
    }

  if (this->has_selected_alternative())
    {
    s << ":alternative=" << p_boot_alternative->r_name;
    }
  }

}}
