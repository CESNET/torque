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

/// \brief Class for holding a single fairshare tree
class FairshareTree
  {
  public:
    /** \brief Construct the fairshare tree with the desired name
     *
     *  Requires the existence of the configuration file resource_group.name
     *
     * @param name Name of the desired tree
     */
    FairshareTree(const std::string& name);
    FairshareTree(const FairshareTree& src); ///< Copy constructor
    ~FairshareTree(); ///< Destructor

    const std::string& get_name() const { return p_name; }  ///< Get the tree name
    const group_info * get_tree() const { return p_tree; }  ///< Get the associated fairshare tree data

    group_info *find_alloc_ginfo(const char *name);         ///< Find a specific fairshare group inside this tree, if not present create it
    group_info *find_alloc_ginfo(const std::string& name);  ///< Find a specific fairshare group inside this tree, if not present create it

    group_info *find_ginfo(const char *name) const;         ///< Find a specific faishare group inside this tree
    group_info *find_ginfo(const std::string& name) const;  ///< Find a specific faishare group inside this tree

    void dump_to_cache() const;   ///< Dump information from this fairshare tree to pbs_cache
    void dump_to_file() const;    ///< Dump information from this fairshare to a file (usage.name)

    void decay(); ///< Decay this fairshare tree

  private:
    std::string p_name; ///< Name of the tree

    int        *p_count; ///< Shared pointer count
    group_info *p_tree;  ///< The actual fairshare data

    group_info* find_ginfo(const char *name, group_info *root) const; ///< Recursive find
    void add_unknown(group_info *ginfo);  ///< Add this group into unknown
    void dump_to_cache(const std::string& metric, group_info *ginfo) const; ///< Recursive store to cache
    void dump_to_file(group_info *root, FILE *fp) const;  ///< Recursive store to file

    void read_configuration();  ///< Read configuration file (resource_group.name)
    void read_usage();          ///< Read usage file (usage.name)

    void decay_tree(group_info *root); ///< Recursively decay the tree
  };

}}

#endif /* FAIRSHARETREE_H_ */
