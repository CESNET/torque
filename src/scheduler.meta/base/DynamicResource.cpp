#include "DynamicResource.h"
#include <cstdlib>

namespace Scheduler {
namespace Base {

DynamicResource::DynamicResource()
  : p_avail(-1), p_assigned(0), p_reported_use(-1), p_scheduled_use(0), p_name("")
  {
  }

DynamicResource::DynamicResource(const char *name, const char *value)
  : p_avail(-1), p_assigned(0), p_reported_use(-1), p_scheduled_use(0), p_name(name)
  {
  char *i1;
  long long int v1 = -1, v2 = -1;
  v1 = strtoll(value,&i1,10);
  if (*i1 == ';') /* there is reported use value */
    {
    i1++;
    char *i2;
    v2 = strtoll(i1,&i2,10);
    if (*i2 != '\0') /* some error */
      v2 = -1;
    }

  p_avail = v1;
  p_reported_use = v2;
  }

void DynamicResource::add_assigned(ResourceBaseType v)
  {
  p_assigned += v;
  }

void DynamicResource::add_scheduled(ResourceBaseType v)
  {
  p_scheduled_use += v;
  }

void DynamicResource::remove_scheduled(ResourceBaseType v)
  {
  p_scheduled_use -= v;
  }

bool DynamicResource::would_fit(ResourceBaseType amount)
  {
  if (p_reported_use >= 0) /* this dynamic resource reports used amount */
    {
    long long int dynamic = p_scheduled_use - p_reported_use;
    if (dynamic < 0) dynamic = 0;

    return p_avail - p_assigned - dynamic >= amount;
    }
  else
    {
    return p_avail - p_assigned >= amount;
    }
  }

}}
