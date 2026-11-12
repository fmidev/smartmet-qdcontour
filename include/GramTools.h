// ======================================================================
/*!
 * \file
 * \brief Interface of namespace GramTools
 */
// ======================================================================
/*!
 * \namespace GramTools
 * \brief Various meteorological objects as paths
 *
 */
// ======================================================================

#ifndef GRAMTOOLS_H
#define GRAMTOOLS_H

#include <imagine/NFmiPath.h>
#include <newbase/NFmiPoint.h>

namespace GramTools
{
Imagine::NFmiPath metarrowflags(float theSpeed, const NFmiPoint &theLatLon);
Imagine::NFmiPath metarrowlines(float theSpeed, const NFmiPoint &theLatLon);

Imagine::NFmiPath metarrowflags(float theSpeed);
Imagine::NFmiPath metarrowlines(float theSpeed);

}  // namespace GramTools

#endif  // GRAMTOOLS_H

// ======================================================================
