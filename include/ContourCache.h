// ======================================================================
/*!
 * \file
 * \brief Interface of class ContourCache
 */
// ======================================================================
/*!
 * \class ContourCache
 * \brief Storage for calculated contours
 *
 * The purpose of the ContourCache is to remember calculated contours
 * and serve them back on demend.
 *
 * Each saved contour is identified by a set of parameters, such
 * as the parameter name, the lower and upper limit of the contour,
 * the time of the data and so on. The set of parameters is by no
 * means completely unique, so one should not change settings too
 * much between rendering on different maps, otherwise wrong results
 * may be drawn.
 *
 * A good principle is to change nothing but the projection, the
 * background and foreground images and the savepath.
 *
 * Typical use is shown below.
 * \code
 * ContourCache cache;
 *
 * NFmiPath path;
 * if(cache.contains(lolimit, hilimit, querydata, time))
 *    path = cache.find(lolimit, hilimit, querydata, time);
 * else
 * {
 *    path = ... some means of calculating it;
 *    cache.insert(path, lolimit, hilimit, querydata, time);
 * }
 * path.Project(area);
 * path.Fill(image, color, rule);
 * \endcode
 */
// ======================================================================

#ifndef CONTOURCACHE_H
#define CONTOURCACHE_H

#include <imagine/NFmiPath.h>

#include <map>
#include <string>

class LazyQueryData;
class NFmiTime;

class ContourCache
{
 private:
  using storage_type = std::map<std::string, Imagine::NFmiPath>;
  storage_type itsData;

 public:
  using size_type = storage_type::size_type;

#ifdef NO_COMPILER_GENERATED
  ~ContourCache();
  ContourCache();
  ContourCache(const ContourCache &theCache);
  ContourCache &operator=(const ContourCache &theCache);
#endif

  bool empty() const;
  void clear();
  size_type size() const;

  bool contains(float theLoLimit,
                float theHiLimit,
                const NFmiTime &theTime,
                const LazyQueryData &theData) const;

  const Imagine::NFmiPath &find(float theLoLimit,
                                float theHiLimit,
                                const NFmiTime &theTime,
                                const LazyQueryData &theData) const;

  void insert(const Imagine::NFmiPath &thePath,
              float theLoLimit,
              float theHiLimit,
              const NFmiTime &theTime,
              const LazyQueryData &theData);

};  // class ContourCache

#endif  // CONTOURCACHE_H

// ======================================================================
