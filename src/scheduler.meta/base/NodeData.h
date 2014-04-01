/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#ifndef NODEDATA_H_
#define NODEDATA_H_

#include "NodeState.h"
#include <set>
#include <string>
#include <vector>

struct resource;

namespace Scheduler {
namespace Base {

enum node_type { NodeTimeshared, NodeCluster, NodeVirtual, NodeCloud };

/* TODO List
 *
 * (1) resource struct has to be rewritten into a class
 * (2) linked list of resources has to be changed into a map<string,resc>?
 * (3) move all set_xyz into private area
 * (4) create constructor that parses a batch response
 */


/** \brief Class for holding and manipulating node data
 *
 */
class NodeData : public Internals::NodeState
  {
    public:
      NodeData();
      ~NodeData();

      void set_phys_props(char *comma_sep_list);  // TODO change to protected / or delete
      void set_virt_props(char *comma_sep_list);  // TODO change to protected / or delete
      void set_priority(char *data);              // TODO change to protected / or delete
      void set_type(char *data);                  // TODO change to protected / or delete
      void set_nomultinode(char *data);           // TODO change to protected / or delete
      void set_nostarving(char *data);            // TODO change to protected / or delete
      void set_proc_total(char *data);            // TODO change to protected / or delete
      void set_proc_free(char *data);             // TODO change to protected / or delete
      void set_ded_queue(char *data);             // TODO change to protected / or delete
      void set_node_cost(const char *value);      // TODO change to protected / or delete
      void set_node_spec(const char *value);      // TODO change to protected / or delete
      void set_avail_before(const char *value);   // TODO change to protected / or delete
      void set_avail_after(const char *value);    // TODO change to protected / or delete
      void set_admin_slot_enabled(const char *value); // TODO change to protected / or delete
      void set_admin_slot_avail(const char *value);   // TODO change to protected / or delete

      void set_resource_capacity(const char *resc, char *value);      // TODO change to protected / or delete
      void set_resource_utilisation(const char *resc, char *value);   // TODO change to protected / or delete
      void set_resource_dynamic(const char *resc, char *value);       // TODO change to protected / or delete

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

      resource *get_resource(const char *name) const;

      const char *get_dedicated_queue_name() const { return p_ded_queue_name; }

      bool has_reg_prop(size_t propid, size_t valueid) const;

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
      char *p_ded_queue_name;

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

      /// \brief Resources
      struct resource *p_resc;

      void add_reg_props(size_t propid, size_t valueid);
      std::vector<size_t> p_reg_props;

  };

}}

#endif /* NODEDATA_H_ */
