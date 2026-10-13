// ======================================================================
/*!
 * \brief Interface of class ImageCache
 */
// ======================================================================

#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <imagine/NFmiImage.h>

#include <map>
#include <string>

class ImageCache
{
 public:
  const Imagine::NFmiImage& getImage(const std::string& theFile) const;
  void clear() const;

 private:
  using storage_type = std::map<std::string, Imagine::NFmiImage>;
  mutable storage_type itsCache;
};

#endif  // IMAGECACHE_H

// ======================================================================
