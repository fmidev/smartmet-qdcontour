// ======================================================================
/*!
 * \brief Tools for handling meridian shifting near 180th meridian
 */
// ======================================================================

#ifndef MERIDIANTOOLS_H
#define MERIDIANTOOLS_H

#include <imagine/NFmiPath.h>
#include <newbase/NFmiArea.h>

namespace MeridianTools
{
NFmiPoint Relocate(const NFmiPoint &thePoint, const NFmiArea &theArea);

void Relocate(Imagine::NFmiPath &thePath, const NFmiArea &theArea);

}  // namespace MeridianTools

#endif  // MERIDIANTOOLS_H

// ======================================================================
