#include "RescInfoDb.h"


#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
using namespace boost::property_tree;
using namespace boost::property_tree::info_parser;
using namespace std;

// global resource information database
RescInfoDb resc_info_db;

void RescInfoDb::read_db(const std::string& filename)
  {
  ptree pt;
  read_info(filename,pt);

  for (ptree::iterator i = pt.begin(); i != pt.end(); i++)
    {
    RescInfo resc;
    resc.name = i->first;
    resc.comment = i->second.get<std::string>("err");

    string type = i->second.get<std::string>("type");

    if (type == "dynamic")
      resc.source = ResCheckDynamic;
    else if (type == "cache")
      resc.source = ResCheckCache;
    else if (type == "status")
      resc.source = ResCheckStat;
    else
      resc.source = ResCheckNone;

    p_resources.insert(make_pair(resc.name,resc));
    }
  }


reschecksource res_check_type(const char * res_name)
  {
  RescInfoDb::iterator i = resc_info_db.find(string(res_name));
  if (i == resc_info_db.end())
    return ResCheckNone;

  return i->second.source;
  }

RescInfoDb::iterator RescInfoDb::begin() { return p_resources.begin(); }
RescInfoDb::iterator RescInfoDb::end() { return p_resources.end(); }

RescInfoDb::const_iterator RescInfoDb::begin() const { return p_resources.begin(); }
RescInfoDb::const_iterator RescInfoDb::end() const { return p_resources.end(); }

RescInfoDb::iterator RescInfoDb::find(const std::string& name) { return p_resources.find(name); }
RescInfoDb::const_iterator RescInfoDb::find(const std::string& name) const { return p_resources.find(name); }

pair<RescInfoDb::iterator,bool> RescInfoDb::insert(const std::string& name, const std::string& comment, reschecksource source)
  {
  RescInfo i;
  i.name = name;
  i.comment = comment;
  i.source = source;
  return p_resources.insert(make_pair(name,i));
  }

pair<RescInfoDb::iterator,bool> RescInfoDb::insert(const char *name, const char *comment, reschecksource source)
  {
  RescInfo i;
  i.name = name;
  i.comment = comment;
  i.source = source;
  return p_resources.insert(make_pair(i.name,i));
  }
