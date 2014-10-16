#include "data_types.h"

#include "misc.h"
#include "base/MiscHelpers.h"

using namespace std;
using namespace Scheduler;
using namespace Base;

Resource::Resource(const string& name) : p_res_type(ResourceStatic), p_name(name), p_str_val(), p_avail(UNSPECIFIED), p_max(INFINITY), p_assign(UNSPECIFIED) {}

Resource::~Resource() {}

void Resource::set_capacity(char *value)
  {
  if (is_num(value))
    {
    this->p_max = res_to_num(value);
    this->p_str_val = value;
    this->p_res_type = ResourceStatic;
    }
  else
    {
    this->p_max = UNSPECIFIED;
    this->p_avail = UNSPECIFIED;
    this->p_assign = UNSPECIFIED;
    this->p_str_val = value;
    this->p_res_type = ResourceString;
    }
  }

void Resource::set_avail(char *value)
  {
  if (is_num(value))
    {
    this->p_avail = res_to_num(value);
    this->p_max = UNSPECIFIED; // dynamic resources don't have capacity
    this->p_str_val = value;
    this->p_res_type = ResourceDynamic;
    }
  else
    {
    this->p_avail = 0;
    this->p_max = UNSPECIFIED;
    this->p_str_val = value;
    this->p_res_type = ResourceString;
    }
  }

void Resource::set_utilization(sch_resource_t value)
  {
  this->p_assign = value;
//  if (this->p_max == UNSPECIFIED)
//    this->p_max = 0;
  }

void Resource::set_utilization(char *value)
  {
  sch_resource_t val = res_to_num(value);
  this->set_utilization(val);
  }

void Resource::consume_resource(char *value)
  {
  sch_resource_t val = res_to_num(value);
  this->consume_resource(val);
  }

void Resource::consume_resource(sch_resource_t value)
  {
  this->p_assign += value;
  }

void Resource::freeup_resource(char *value)
  {
  sch_resource_t val = res_to_num(value);
  this->freeup_resource(val);
  }

void Resource::freeup_resource(sch_resource_t value)
  {
  this->p_assign -= value;
  }

void Resource::reset()
  {
  this->p_res_type = ResourceStatic;
  this->p_avail = UNSPECIFIED;
  this->p_max = UNSPECIFIED;
  this->p_assign = UNSPECIFIED;
  }

CheckResult Resource::check_numeric_fit(sch_resource_t amount)
  {
  if (this->p_max != INFINITY && this->p_max != UNSPECIFIED && this->p_max < amount) // there is a max and it's lower than the requested amount
    return CheckNonFit;

  if (this->p_max == UNSPECIFIED || this->p_max == INFINITY) // no max value present, only current
    {
    if (this->p_avail - this->p_assign >= amount)
      return CheckAvailable;
    else
      return CheckOccupied;
    }
  else // max value present
    {
    if (this->p_max - this->p_assign >= amount)
      return CheckAvailable;
    else
      return CheckOccupied;
    }
  }

CheckResult Resource::check_fit(char *value)
  {
  if (this->is_string()) // string resources work as properties
    {
    if (p_str_val != value)
      return CheckNonFit;
    else
      return CheckAvailable;
    }

  sch_resource_t amount = res_to_num(value);

  return this->check_numeric_fit(amount);
  }
