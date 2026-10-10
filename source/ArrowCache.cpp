// ======================================================================
/*!
 * \file
 * \brief Implementation of class ArrowCache
 */
// ======================================================================

#include "ArrowCache.h"
#include <fstream>
#include <stdexcept>

// <newbase/NFmixxx.h> won't be good for Windows compilation off cvs checkout
// dir.
//
#include <newbase/NFmiStringTools.h>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Test if the cache is empty
 */
// ----------------------------------------------------------------------

bool ArrowCache::empty() const { return itsCache.empty(); }
// ----------------------------------------------------------------------
/*!
 * \brief Clear the cache
 */
// ----------------------------------------------------------------------

void ArrowCache::clear() { itsCache.clear(); }
// ----------------------------------------------------------------------
/*!
 * \brief Return the desired arrow from the cache
 */
// ----------------------------------------------------------------------

const string &ArrowCache::find(const string &theName)
{
  auto it = itsCache.find(theName);
  if (it != itsCache.end()) return it->second;

  ifstream arrow(theName.c_str());
  if (!arrow) throw runtime_error("Could not open arrow '" + theName + "' for reading");
  string pathstring = NFmiStringTools::ReadFile(arrow);
  arrow.close();

  pair<cache_type::const_iterator, bool> ret =
      itsCache.insert(cache_type::value_type(theName, pathstring));

  if (!ret.second)
    throw runtime_error("Failed to insert arrow '" + theName + "' into the internal cache");

  return ret.first->second;
}
