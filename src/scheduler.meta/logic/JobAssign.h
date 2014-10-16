#ifndef JOBASSIGN_H_
#define JOBASSIGN_H_

extern "C" {
#include "site_pbs_cache_common.h"
#include "nodespec.h"
}

#include <iosfwd>
#include "LogicFwd.h"

namespace Scheduler {
namespace Logic {

/// \brief Class for storing a job-2-node assignment during scheduling process
class JobAssign
  {
  public:
    bool has_fairshare_flag() const;
    void unset_fairshare_flag();
    void set_fairshare_flag();

    ScratchType get_scratch_assign() const;
    void set_scratch_assign(ScratchType scratch_type);

    void set_selected_alternative(repository_alternatives *boot_alternative);
    bool has_selected_alternative() const;
    repository_alternatives *get_selected_alternative() const;

    bool has_assignment() const;
    pars_spec_node *get_assignment() const;
    void set_assignment(pars_spec_node *spec);

    void clean_assign();

    JobAssign();
    ~JobAssign();

    void get_assign_string(std::stringstream& s, const char *node_name, AssignStringMode mode) const;

    bool has_virtual_assignment() const;
    void set_virtual_assignment();

  private:
    bool p_flag_fairshare;
    ScratchType p_scratch_type;
    repository_alternatives *p_boot_alternative;
    pars_spec_node *p_nodespec;
    bool p_virtual_assignment;
  };

}}

#endif
