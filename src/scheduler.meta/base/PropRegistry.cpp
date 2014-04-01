/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#include <base/PropRegistry.h>
using namespace std;

namespace Scheduler {
namespace Base {

pair<size_t,size_t> PropRegistry::register_property(const char* property, const char* value)
  {
  return this->register_property(string(property),string(value));
  }

pair<size_t,size_t> PropRegistry::register_property(const string& property, const string& value)
  {
  map<string,size_t>::iterator iprop = p_properties.find(property);
  size_t index = 0;

  if (iprop == p_properties.end()) // property not present, register
    {
    p_prop_index.push_back(property);
    index = p_prop_index.size()-1;
    p_properties.insert(make_pair(property,index));
    p_values.push_back(map<string,size_t>());
    p_val_index.push_back(vector<string>());
    }
  else
    {
    index = iprop->second;
    }

  map<string,size_t>::iterator ival = p_values[index].find(value);
  size_t vindex = 0;

  if (ival == p_values[index].end()) // value not present, register
    {
    p_val_index[index].push_back(value);
    vindex = p_val_index[index].size()-1;
    p_values[index].insert(make_pair(value,vindex));
    }
  else
    {
    vindex = ival->second;
    }

  return make_pair(index,vindex);
  }

pair<bool,size_t> PropRegistry::get_property_id(const char *property) const
  {
  return this->get_property_id(string(property));
  }

pair<bool,size_t> PropRegistry::get_property_id(const string& property) const
  {
  map<string,size_t>::const_iterator iprop = p_properties.find(property);

  if (iprop == p_properties.end())
    return make_pair(false,static_cast<size_t>(0));

  return make_pair(true,iprop->second);
  }

pair<bool,size_t> PropRegistry::get_value_id(size_t property_id, const char *value) const
  {
  return this->get_value_id(property_id,string(value));
  }

pair<bool,size_t> PropRegistry::get_value_id(size_t property_id, const string& value) const
  {
  if (property_id >= p_prop_index.size())
    return make_pair(false,size_t(0));

  map<string,size_t>::const_iterator ival = p_values[property_id].find(value);
  if (ival == p_values[property_id].end())
    return make_pair(false,size_t(0));

  return make_pair(true,ival->second);
  }

size_t PropRegistry::prop_count() const
  {
  return p_prop_index.size();
  }

pair<bool,size_t> PropRegistry::value_count(size_t property_id) const
  {
  if (property_id >= p_prop_index.size())
    return make_pair(false,size_t(0));

  return make_pair(true,p_val_index[property_id].size());
  }

static PropRegistry *registry = NULL;

PropRegistry *get_prop_registry()
  {
  if (registry == NULL)
    registry = new PropRegistry();

  return registry;
  }

}}
