#include "ResourceSet.h"
#include "boost/make_shared.hpp"
using namespace std;
using namespace boost;
using namespace Scheduler;
using namespace Base;

boost::shared_ptr<Resource> ResourceSet::get_resource(const string& name) const
  {
  map<string,boost::shared_ptr<Resource> >::const_iterator i = p_resc.find(name);
  if (i == p_resc.end())
    return boost::shared_ptr<Resource>();

  return i->second;
  }

boost::shared_ptr<Resource> ResourceSet::get_alloc_resource(const string& name)
  {
  map<string,boost::shared_ptr<Resource> >::iterator i = p_resc.find(name);
  if (i == p_resc.end())
    {
    boost::shared_ptr<Resource> result = boost::make_shared<Resource>(name);
    p_resc.insert(make_pair(name,result));
    return result;
    }

  return i->second;
  }

ResourceSet::ResourceSet() : p_resc() {}

ResourceSet::ResourceSet(const ResourceSet& src) : p_resc()
  {
  map<string,boost::shared_ptr<Resource> >::const_iterator i = src.p_resc.begin();
  while (i != src.p_resc.end())
    {
    p_resc.insert(make_pair(i->first,boost::make_shared<Resource>(*(i->second))));
    i++;
    }
  }
