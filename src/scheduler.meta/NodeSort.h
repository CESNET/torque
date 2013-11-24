#ifndef NODESORT_H_
#define NODESORT_H_

#include "data_types.h"

/** \brief Sort nodes by current state */
struct NodeStateSort
  {
  bool operator()(node_info* left, node_info* right) const;
  };

/** \brief Sort nodes by fairshare cost */
struct NodeCostSort
  {
  NodeCostSort(unsigned procs, unsigned long long mem);
  bool operator ()(node_info *left, node_info *right) const;

  private:
    unsigned p_procs;
    unsigned long long p_mem;
  };

#endif /* NODESORT_H_ */
