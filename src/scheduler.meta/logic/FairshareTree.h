#ifndef FAIRSHARETREE_H_
#define FAIRSHARETREE_H_

#include <string>
#include "data_types.h"

/// \brief Legacy Fairshare Structure
struct group_info
  {
  char *name;    /* name of user/group */
  int resgroup;    /* resgroup the group is in */
  int cresgroup;   /* resgroup of the children of group */
  int shares;    /* number of shares this group has */
  float percentage;   /* overall percentage the group has */
  usage_t usage;   /* calculated usage info */
  usage_t temp_usage;   /* usage plus any temporary usage */

  group_info *parent;   /* parent node */
  group_info *sibling;   /* sibling node */
  group_info *child;   /* child node */
  };

/// \brief Legacy Fairshare Structure for usage file
struct group_node_usage
  {
  char name[9];
  usage_t usage;
  };

namespace Scheduler {
namespace Logic {

/** \brief Class for holding one fairshare tree
 *
 */
class FairshareTree
  {
  public:
    FairshareTree(const std::string& name);
    FairshareTree(const FairshareTree& src);
    ~FairshareTree();

    const std::string& get_name() const { return p_name; }
    const group_info * get_tree() const { return p_tree; }

    group_info *find_alloc_ginfo(const char *name);
    group_info *find_alloc_ginfo(const std::string& name);

    group_info *find_ginfo(const char *name) const;
    group_info *find_ginfo(const std::string& name) const;

    void dump_to_cache() const;
    void dump_to_file() const;

    void decay();

  private:
    std::string p_name;

    int        *p_count; ///< Shared pointer count
    group_info *p_tree;  ///< The actual fairshare data

    group_info* find_ginfo(const char *name, group_info *root) const;
    void add_unknown(group_info *ginfo);
    void dump_to_cache(const std::string& metric, group_info *ginfo) const;
    void dump_to_file(group_info *root, FILE *fp) const;

    void read_configuration();
    void read_usage();

    void decay_tree(group_info *root);
  };

}}

#endif /* FAIRSHARETREE_H_ */
