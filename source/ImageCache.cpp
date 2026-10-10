// ======================================================================
/*!
 * \brief Implementation of class ImageCache
 */
// ======================================================================

#include "ImageCache.h"

//#include <iostream>

using namespace Imagine;
using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Clear the cache
 */
// ----------------------------------------------------------------------

void ImageCache::clear() const { itsCache.clear(); }
// ----------------------------------------------------------------------
/*!
 * \brief Find image from cache (or read it if necessary)
 */
// ----------------------------------------------------------------------

const ImagineXr_or_NFmiImage &ImageCache::getImage(const string &theFile) const
{
  auto it = itsCache.find(theFile);
  if (it != itsCache.end()) return it->second;

  pair<storage_type::const_iterator, bool> ret =
      itsCache.insert(storage_type::value_type(theFile, ImagineXr_or_NFmiImage(theFile)));

  if (ret.second) return ret.first->second;

  throw runtime_error("ImageCache failed to store '" + theFile + "'");
}
