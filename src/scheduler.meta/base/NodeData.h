/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#ifndef NODEDATA_H_
#define NODEDATA_H_

#include "BaseFwd.h"

#include "NodeState.h"
#include <set>
#include <string>
#include <vector>
#include "torque.h"

#include "ResourceSet.h"

namespace Scheduler {
namespace Base {

enum node_type { NodeTimeshared, NodeCluster, NodeVirtual, NodeCloud };

/** \brief Class for holding and manipulating node data
 *
 * This class provides storage for all the PBS server handled node data.
 */
class NodeData : public Internals::NodeState
  {
    public:
      NodeData(struct batch_status *node_data);
      ~NodeData();

      bool has_virt_prop(const char *name) const;
      bool has_phys_prop(const char *name) const;

      long get_priority() const { return p_priority; }
      node_type get_type() const { return p_type; }
      bool get_nomultinode() const { return p_no_multinode; }
      bool get_nostarving() const { return p_no_starving; }

      int get_cores_total() const { return p_core_total; }
      int get_cores_free() const { return p_core_free; }

      bool get_admin_slot_avail() const { return p_admin_slot_avail; }
      bool get_admin_slot_enabled() const { return p_admin_slot_enabled; }

      double get_node_cost() const { return p_node_cost; }
      double get_node_spec() const { return p_node_spec; }

      long get_avail_before() const { return p_avail_before; }
      long get_avail_after() const { return p_avail_after; }

      const std::string& get_scratch_pool() const { return p_scratch_pool; }

      Resource *get_resource(const char *name) const;

      const std::string& get_dedicated_queue_name() const { return p_ded_queue_name; }

      bool has_reg_prop(size_t propid, size_t valueid) const;

      const char *get_name() const;

      bool has_jobs() const;
      bool is_exclusively_assigned() const;

      void set_parent_server(server_info* server) { p_server = server; }
      server_info* get_parent_server() const { return p_server; }

    private:
      /// \brief Node physical properties
      std::set<std::string> p_phys_props;
      /// \brief Node virtual properties
      std::set<std::string> p_virt_props;
      /// \brief Node priority
      long p_priority;
      /// \brief Type of the node (cluster,timeshared,virtual,cloud)
      node_type p_type;
      /// \brief Prevent multinode jobs on this node
      bool p_no_multinode;
      /// \brief Prevent starvation reservations on this node
      bool p_no_starving;
      /// \brief Total number of cores on node
      int p_core_total;
      /// \brief Number of free cores on node
      int p_core_free;
      /// \brief Name of queue this node is dedicated to
      std::string p_ded_queue_name;
      /// \brief Is admin slot enabled
      bool p_admin_slot_enabled;
      /// \brief Is admin slot available
      bool p_admin_slot_avail;
      /// \brief Node fairshare cost
      double p_node_cost;
      /// \brief Node fairshare machine spec
      double p_node_spec;
      /// \brief Node available before
      long p_avail_before;
      /// \brief Node available after
      long p_avail_after;
      /// \brief Node resource list
      ResourceSet p_resc;

      void add_reg_props(size_t propid, size_t valueid);
      std::vector<size_t> p_reg_props;

      /// \brief Node name
      std::string p_name;
      /// \brief Set of jobs currently running on node
      std::set<std::string> p_jobs;
      /// \brief Is node exclusively assigned
      bool p_exclusively_assigned;

      /// \brief Shared scratch pool
      std::string p_scratch_pool;

      /// \brief Parent server
      server_info *p_server;

    public:
      void set_resource_dynamic(const char *resc, char *value);
      void set_scratch_pool(const char *pool);

    private:
      void set_phys_props(char *comma_sep_list);
      void set_virt_props(char *comma_sep_list);
      void set_priority(char *data);
      void set_type(char *data);
      void set_nomultinode(char *data);
      void set_nostarving(char *data);
      void set_proc_total(char *data);
      void set_proc_free(char *data);
      void set_ded_queue(const char *data);
      void set_node_cost(const char *value);
      void set_node_spec(const char *value);
      void set_avail_before(const char *value);
      void set_avail_after(const char *value);
      void set_admin_slot_enabled(const char *value);
      void set_admin_slot_avail(const char *value);
      void set_resource_capacity(const char *resc, char *value);
      void set_resource_utilisation(const char *resc, char *value);
      void set_name(const char *name);
      void set_jobs(char *comma_sep_list);
      void set_exclusively_assigned(const char* value);
  };

}}

#endif /* NODEDATA_H_ */
