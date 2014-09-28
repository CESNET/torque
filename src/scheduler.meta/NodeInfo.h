#ifndef NODEINFO_H_
#define NODEINFO_H_

#include <vector>
#include <set>
#include <map>
#include <string>
#include <cstdlib>
#include <cstring>

enum ResourceCheckMode { MaxOnly, Avail };

#include "logic/JobAssign.h"
#include "logic/NodeLogic.h"
using namespace Scheduler;
using namespace Base;
using namespace Logic;
#include "JobInfo.h"

struct node_info : public NodeLogic, public JobAssign // change to protected inheritance
  {
  /// Test not-usable state
  bool is_notusable() const { return p_is_notusable; }
  /// Mark state as not usable
  void set_notusable() { p_is_notusable = true; }

  void set_excl_queue(queue_info *queue) { p_excl_queue = queue; }

  node_info *get_host() const { return p_host; }
  void set_host(node_info *host) { p_host = host; }

  std::vector<node_info*>& get_hosted() { return p_hosted; }
  const std::vector<node_info*>& get_hosted() const { return p_hosted; }

  void set_virtual_image(const std::string& image) { p_virtual_image = image; }
  const std::string& get_virtual_image() const { return p_virtual_image; }

  void set_virtual_cluster(const std::string& cluster) { p_virtual_cluster = cluster; }
  const std::string& get_virtual_cluster() const { return p_virtual_cluster; }

  void set_phys_cluster(const std::string& cluster) { p_phys_cluster = cluster; }
  const std::string& get_phys_cluster() const { return p_phys_cluster; }

  bool is_building_cluster() const { return p_is_building_cluster; }




  MagratheaState magrathea_status;
  struct repository_alternatives ** alternatives;

  CheckResult has_prop(const pars_prop* property, bool physical_only) const;
  bool has_prop(const char* property) const;

  void get_assign_string(std::stringstream& s, AssignStringMode mode) const
    { JobAssign::get_assign_string(s,this->get_name(),mode); }

  /** \brief Check whether the job will fit on the node processors
   *
   * Check whether there are enough processors to match the specified job/nodespec to this node.
   * Also handles admin slots and exclusive requests.
   */
  CheckResult has_spec(const job_info *job, const pars_spec_node *spec, ScratchType *scratch) const;
  CheckResult has_props_boot(const job_info *job, const pars_spec_node *spec, const repository_alternatives *virt_conf) const;
  CheckResult has_props_run(const job_info *job, const pars_spec_node *spec) const;

  CheckResult has_bootable_state(enum ClusterMode mode) const;
  CheckResult has_runnable_state() const;

  CheckResult can_run_job(const job_info *jinfo) const;
  CheckResult can_boot_job(const job_info *jinfo) const;

  CheckResult can_fit_job_for_run(const job_info *jinfo, const pars_spec_node *spec, ScratchType *scratch) const;
  CheckResult can_fit_job_for_boot(const job_info *jinfo, const pars_spec_node *spec, ScratchType *scratch, repository_alternatives **alternative) const;

  void deplete_admin_slot();
  void deplete_exclusive_access();

  unsigned long long get_mem_total() const;

  void fetch_bootable_alternatives();

  bool operator < (const node_info& right);

  void process_magrathea_status();
  void process_machine_cluster();

  void expand_virtual_nodes()
    {
    // nodes cannot be expanded once assigned
    assert((!this->has_assignment()) && (!this->has_starving_assignment()));

    }

private:
  node_info& operator = (const node_info& src) { return *this; }

public:
  node_info(struct batch_status *node_data) : NodeLogic(node_data), alternatives(NULL), p_is_notusable(false), p_is_rebootable(false), p_is_building_cluster(false), p_virtual_image(), p_virtual_cluster(), p_phys_cluster(), p_excl_queue(NULL), p_host(NULL), p_hosted()
  {
    p_hosted.reserve(2);
  }
  ~node_info() { free_bootable_alternatives(alternatives); }

  node_info(const node_info& src) : NodeLogic(src), JobAssign(src), magrathea_status(src.magrathea_status), alternatives(NULL),
                                    p_is_notusable(src.p_is_notusable), p_is_rebootable(src.p_is_rebootable), p_is_building_cluster(src.p_is_building_cluster),
                                    p_virtual_image(src.p_virtual_image), p_virtual_cluster(src.p_virtual_cluster), p_phys_cluster(src.p_phys_cluster),
                                    p_excl_queue(src.p_excl_queue), p_host(src.p_host), p_hosted(src.p_hosted)

    {
    if (src.alternatives != NULL)
      alternatives = dup_bootable_alternatives(src.alternatives);
    else
      alternatives = NULL;
    }


  private:
    bool p_is_notusable;
    bool p_is_rebootable;
    bool p_is_building_cluster;

    std::string p_virtual_image;
    std::string p_virtual_cluster;
    std::string p_phys_cluster;

    queue_info *p_excl_queue; /**< pointer to queue the node is exclusive to */
    node_info *p_host; /*< the physical host of this node */
    std::vector<node_info*> p_hosted; /*< virtual nodes hosted on this node */

  };

#endif /* NODEINFO_H_ */
