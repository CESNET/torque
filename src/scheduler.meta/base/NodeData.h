/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#ifndef NODEDATA_H_
#define NODEDATA_H_

#include "NodeState.h"
#include <set>
#include <string>

namespace Scheduler {
namespace Base {

enum node_type { NodeTimeshared, NodeCluster, NodeVirtual, NodeCloud };

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

      bool has_virt_prop(const char *name) const;
      bool has_phys_prop(const char *name) const;

      long get_priority() const { return p_priority; }
      node_type get_type() const { return p_type; }
      bool get_nomultinode() const { return p_no_multinode; }
      bool get_nostarving() const { return p_no_starving; }

      int get_cores_total() const { return p_core_total; }
      int get_cores_free() const { return p_core_free; }

      const char *get_dedicated_queue_name() const { return p_ded_queue_name; }

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

  };

}}

#endif /* NODEDATA_H_ */
