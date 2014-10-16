#ifndef RESOURCE_H_
#define RESOURCE_H_

#include "base/BaseFwd.h"

enum ResourceType { ResourceStatic, ResourceDynamic, ResourceString };

namespace Scheduler {
namespace Base {

class Resource
  {
  public:
    Resource(const std::string& name);
    ~Resource();

    void set_capacity(char *value);
    void set_avail(char *value);
    void set_utilization(char *value);
    void set_utilization(sch_resource_t value);

    void consume_resource(char *value);
    void consume_resource(sch_resource_t value);

    void freeup_resource(char *value);
    void freeup_resource(sch_resource_t value);

    void reset();

    CheckResult check_fit(char *value);
    CheckResult check_numeric_fit(sch_resource_t value);

    bool is_string() { return p_res_type == ResourceString; }
    const std::string& get_name() const { return p_name; }
    void set_name(const std::string& name) { p_name = name; }

    const std::string& get_str_val() const { return p_str_val; }

    sch_resource_t get_capacity() const { return p_max; }
    sch_resource_t get_utilization() const { return p_assign; }

  private:
    ResourceType p_res_type;
    std::string  p_name;
    std::string  p_str_val;

    sch_resource_t p_avail; // currently available - used for dynamic resources (without max. cap.)
    sch_resource_t p_max;   // capacity - used by static resources
    sch_resource_t p_assign; // currently assigned amount (allocated & scheduled)

  };

}}

#endif /* RESOURCE_H_ */
