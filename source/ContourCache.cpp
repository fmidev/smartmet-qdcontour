// ======================================================================
/*!
 * \file
 * \brief Implementation of class ContourCache
 */
// ======================================================================

#include "ContourCache.h"
#include "LazyQueryData.h"

#include <newbase/NFmiTime.h>

#include <sstream>

using namespace std;

namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Return a cache-key for the given contour settings
 *
 * \param theLoLimit The lower limit of the contour
 * \param theHiLimit The upper limit of the contour
 * \param theTime The actual data time which may be interpolated
 * \param theData The query data
 * \return The key for the data in the cache
 */
// ----------------------------------------------------------------------

std::string cache_key(float theLoLimit,
                      float theHiLimit,
                      const NFmiTime &theTime,
                      const LazyQueryData &theData)
{
  ostringstream os;

  os << theLoLimit << '_' << theHiLimit << '_' << theData.Filename() << '_'
     << theTime.ToStr(kYYYYMMDDHHMM).CharPtr() << '_'
     << theData.OriginTime().ToStr(kYYYYMMDDHHMM).CharPtr() << '_' << theData.GetParamName() << '_'
     << theData.GetParamIdent() << '_' << theData.GetLevelNumber();

  return os.str();
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Test if the cache is empty
 *
 * \return True if the cache is empty
 */
// ----------------------------------------------------------------------

bool ContourCache::empty() const { return itsData.empty(); }
// ----------------------------------------------------------------------
/*!
 * \brief Empty the cache
 */
// ----------------------------------------------------------------------

void ContourCache::clear() { itsData.clear(); }
// ----------------------------------------------------------------------
/*!
 * \brief Return the number of cached contours
 *
 * \return The number of cached contours
 */
// ----------------------------------------------------------------------

ContourCache::size_type ContourCache::size() const { return itsData.size(); }
// ----------------------------------------------------------------------
/*!
 * \brief Test if given contour is cached
 *
 * \param theLoLimit The lower limit of the contour
 * \param theHiLimit The upper limit of the contour
 * \param theTime The actual data time may be interpolated (<> ValidTime)
 * \param theData The query data
 */
// ----------------------------------------------------------------------

bool ContourCache::contains(float theLoLimit,
                            float theHiLimit,
                            const NFmiTime &theTime,
                            const LazyQueryData &theData) const
{
  string key = cache_key(theLoLimit, theHiLimit, theTime, theData);
  storage_type::const_iterator it = itsData.find(key);
  return (it != itsData.end());
}

// ----------------------------------------------------------------------
/*!
 * \brief Find the given cached contour
 *
 * This will throw if the contour is not cached
 *
 * \param theLoLimit The lower limit of the contour
 * \param theHiLimit The upper limit of the contour
 * \param theTime The actual data time may be interpolated (<> ValidTime)
 * \param theData The query data
 * \return The path
 */
// ----------------------------------------------------------------------

const Imagine::NFmiPath &ContourCache::find(float theLoLimit,
                                            float theHiLimit,
                                            const NFmiTime &theTime,
                                            const LazyQueryData &theData) const
{
  string key = cache_key(theLoLimit, theHiLimit, theTime, theData);
  storage_type::const_iterator it = itsData.find(key);
  if (it != itsData.end()) return it->second;
  throw runtime_error("Contour was not in the cache - use contains first!");
}

// ----------------------------------------------------------------------
/*!
 * \brief Insert a new path into the cache
 *
 * This will throw if the contour is already cached
 *
 * \param thePath The path to cache
 * \param theLoLimit The lower limit of the contour
 * \param theHiLimit The upper limit of the contour
 * \param theTime The actual data time may be interpolated (<> ValidTime)
 * \param theData The query data
 */
// ----------------------------------------------------------------------

void ContourCache::insert(const Imagine::NFmiPath &thePath,
                          float theLoLimit,
                          float theHiLimit,
                          const NFmiTime &theTime,
                          const LazyQueryData &theData)
{
  string key = cache_key(theLoLimit, theHiLimit, theTime, theData);

  typedef pair<storage_type::const_iterator, bool> restype;

  restype result = itsData.insert(storage_type::value_type(key, thePath));

  if (!result.second) throw runtime_error("Contour was already in the cache!");
}

// ======================================================================
