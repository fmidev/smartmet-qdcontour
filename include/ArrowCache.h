// ======================================================================
/*!
 * \file
 * \brief Interface of class ArrowCache
 */
// ======================================================================

#ifndef ARROWCACHE_H
#define ARROWCACHE_H

#include <map>
#include <string>

class ArrowCache
{
 public:
  const std::string &find(const std::string &theName);
  bool empty() const;
  void clear();

 private:
  using cache_type = std::map<std::string, std::string>;
  cache_type itsCache;

};  // class ArrowCache

#endif
