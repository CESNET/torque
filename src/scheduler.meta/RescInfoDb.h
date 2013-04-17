#ifndef RESCINFODB_H_
#define RESCINFODB_H_

#include <string>
#include <map>

/** resources check mode */
typedef enum { ResCheckNone, ResCheckStat, ResCheckCache, ResCheckBoth, ResCheckDynamic } reschecksource;

struct RescInfo
  {
  std::string name;
  std::string comment;

  reschecksource source;
  };

class RescInfoDb
  {
  public:
    void read_db(const std::string& filename);
    typedef std::map<std::string,RescInfo>::iterator iterator;
    typedef std::map<std::string,RescInfo>::const_iterator const_iterator;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    iterator find(const std::string& name);
    const_iterator find(const std::string& name) const;

    std::pair<iterator,bool> insert(const std::string& name, const std::string& comment, reschecksource source);
    std::pair<iterator,bool> insert(const char *name, const char *comment, reschecksource source);

  private:
    std::map<std::string,RescInfo> p_resources;
  };

extern RescInfoDb resc_info_db;
reschecksource res_check_type(const char *res_name);

#endif /* RESCINFODB_H_ */
