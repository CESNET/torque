/* (c) 2014 Simon Toth (kontakt@simontoth.cz) for CESNET a.l.e.
 * Licensed under MIT license http://opensource.org/licenses/MIT
 */

#ifndef PROPREGISTRY_H_
#define PROPREGISTRY_H_

#include <string>
#include <utility>
#include <vector>
#include <map>

namespace Scheduler {
namespace Base {

/** \brief Property registry
 *
 * This class is capable of registering and storing information about the existing string resource combinations (resource <-> value).
 *
 * This speeds up scheduling as it assigns an unique ID to each resource and then an unique ID for each value within that resource.
 * Scheduler can then simply store the combinations as an integer array where the array index is the resource ID and the value is the value ID.
 */
class PropRegistry
  {
    public:

      /** \brief Register a new property and its value
       *
       * Function will correctly register either both property and
       * value or just value as necessary, or simply return indexes
       * when both property and value are already registered.
       *
       * @param property Property name to register
       * @param value Value to register
       * @return property and value index
       */
      std::pair<size_t, size_t> register_property(const char *property, const char *value);

      /** \brief Register a new property and its value
       *
       * Function will correctly register either both property and
       * value or just value as necessary, or simply return indexes
       * when both property and value are already registered.
       *
       * @param property Property name to register
       * @param value Value to register
       * @return property and value index
       */
      std::pair<size_t, size_t> register_property(const std::string& property, const std::string& value);

      /** \brief Search for a registered property
       *
       * @param property Property name to find
       * @return a pair determining whether property is present and if present, on which index
       */
      std::pair<bool,size_t> get_property_id(const char *property) const;

      /** \brief Search for a registered property
       *
       * @param property Property name to find
       * @return a pair determining whether property is present and if present, on which index
       */
      std::pair<bool,size_t> get_property_id(const std::string& property) const;

      /** \brief Search for a registered value belonging to a specific property
       *
       * @param property Value to find
       * @return a pair determining whether the value is present and if present, on which index
       */
      std::pair<bool,size_t> get_value_id(size_t property_id, const char *value) const;

      /** \brief Search for a registered value belonging to a specific property
       *
       * @param property Value to find
       * @return a pair determining whether the value is present and if present, on which index
       */
      std::pair<bool,size_t> get_value_id(size_t property_id, const std::string& value) const;

      /** \brief Get the number of registered properties
       *
       * @return number of registered properties
       */
      size_t prop_count() const;

      /** \brief Get the number of registered values for a particular property
       *
       * @return pair of a boolean (false if property was not found) and the number of registered values
       */
      std::pair<bool,size_t> value_count(size_t property_id) const;

    private:
      std::map<std::string,size_t> p_properties;
      std::vector<std::string> p_prop_index;
      std::vector< std::map<std::string,size_t> > p_values;
      std::vector< std::vector<std::string> > p_val_index;
  };

PropRegistry *get_prop_registry();

}}

#endif /* PROPREGISTRY_H_ */
