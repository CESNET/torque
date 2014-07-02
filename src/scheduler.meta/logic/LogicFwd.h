/** \file This is a forward header for all logic classes/headers */
#ifndef LOGICFWD_H_
#define LOGICFWD_H_

// Fairshare Tree
struct group_info;
struct group_node_usage;

namespace Scheduler {
namespace Logic {

// Fairshare Tree
class FairshareTree;

// JobAssign
enum AssignStringMode { FullAssignString, RescOnlyAssignString, NumericOnlyAssignString };
class JobAssign;

// NodeFilters
enum SuitableNodeFilterMode { SuitableAssignMode, SuitableFairshareMode, SuitableStarvingMode, SuitableRebootMode };
struct NodeSuitableForJob;
struct NodeSuitableForSpec;
struct NodeSuitableForPlace;

// NodeLogic
enum CheckResult { CheckAvailable, CheckOccupied, CheckNonFit };
class NodeLogic;

}}

#endif /* LOGICFWD_H_ */
