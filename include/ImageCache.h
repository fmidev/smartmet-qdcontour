// ======================================================================
/*!
 * \brief Interface of class ImageCache
 */
// ======================================================================

#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <imagine/imagine-config.h>
#ifdef IMAGINE_WITH_CAIRO
#include "ImagineXr.h"
#endif

#include <imagine/NFmiImage.h>

#include <map>
#include <string>

class ImageCache
{
 public:
  const ImagineXr_or_NFmiImage &getImage(const std::string &theFile) const;
  void clear() const;

 private:
  using storage_type = std::map<std::string, ImagineXr_or_NFmiImage>;
  mutable storage_type itsCache;
};

#endif  // IMAGECACHE_H

// ======================================================================
