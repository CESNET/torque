#include "NodeSort.h"

using namespace std;

bool NodeStateSort::operator()(node_info* left, node_info* right) const
  {
  return *left < *right;
  }

NodeCostSort::NodeCostSort(unsigned procs, unsigned long long mem, bool exclusive) : p_procs(procs), p_mem(mem), p_excl(exclusive) {}

bool NodeCostSort::operator ()(node_info *left, node_info *right) const
  {
  double left_cost = 0;
  double right_cost = 0;

  int left_cores = left->get_cores_total();
  int right_cores = right->get_cores_total();

  if (p_excl)
    {
    left_cost = left->node_cost*left_cores;
    right_cost = right->node_cost*right_cores;
    }
  else
    {
    double left_perc = max(static_cast<double>(p_procs)/left_cores,static_cast<double>(p_mem)/left->get_mem_total())*left->node_cost;
    double right_perc = max(static_cast<double>(p_procs)/right_cores,static_cast<double>(p_mem)/right->get_mem_total())*right->node_cost;

    left_cost = left_perc*left_cores;
    right_cost = right_perc*right_cores;
    }

  if (left_cost < right_cost)
    return true;
  else
    return false;
  }
