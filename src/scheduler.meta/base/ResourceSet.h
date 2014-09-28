#ifndef RESOURCESET_H_
#define RESOURCESET_H_

#include <map>
#include <string>
#include "base/Resource.h"
#include "boost/shared_ptr.hpp"

namespace Scheduler {
namespace Base {

class ResourceSet
  {
  public:
    boost::shared_ptr<Resource> get_resource(const std::string& name) const;
    boost::shared_ptr<Resource> get_alloc_resource(const std::string& name);

    ResourceSet();
    ResourceSet(const ResourceSet& src);

  private:
    std::map<std::string, boost::shared_ptr<Resource> > p_resc;

    ResourceSet& operator = (const ResourceSet& src) { return *this; }
  };

}}

#endif /* RESOURCESET_H_ */
