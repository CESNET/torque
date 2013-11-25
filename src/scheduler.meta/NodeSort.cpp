#include "NodeSort.h"

using namespace std;

bool NodeStateSort::operator()(node_info* left, node_info* right) const
  {
  return *left < *right;
  }

NodeCostSort::NodeCostSort(unsigned procs, unsigned long long mem) : p_procs(procs), p_mem(mem) {}

bool NodeCostSort::operator ()(node_info *left, node_info *right) const
  {
  double left_perc = max(static_cast<double>(p_procs)/left->get_proc_total(),static_cast<double>(p_mem)/left->get_mem_total())*left->node_cost;
  double right_perc = max(static_cast<double>(p_procs)/right->get_proc_total(),static_cast<double>(p_mem)/right->get_mem_total())*right->node_cost;

  if (left_perc*left->get_proc_total() < right_perc*right->get_proc_total())
    return true;
  else
    return false;
  }
