#ifndef DYNAMICRESOURCE_H_
#define DYNAMICRESOURCE_H_

#include "BaseTypes.h"
#include <string>

namespace Scheduler {
namespace Base {

/** \brief Dynamic resource helper class */
class DynamicResource
  {
  ResourceBaseType  p_avail;
  ResourceBaseType  p_assigned;
  ResourceBaseType  p_reported_use;
  ResourceBaseType  p_scheduled_use;
  std::string       p_name;

  public:
    DynamicResource();
    /** \brief Construct from a string in the format: "avail;used" */
    DynamicResource(const char *name, const char *value);

    void add_assigned(ResourceBaseType v);
    void add_scheduled(ResourceBaseType v);
    void remove_scheduled(ResourceBaseType v);

    /** \brief Test whether the specified amount can be scheduled to a job */
    bool would_fit(ResourceBaseType amount);
  };

}}

#endif /* DYNAMICRESOURCE_H_ */
