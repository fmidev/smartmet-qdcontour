// ======================================================================
/*!
 * \file
 * \brief Interface of namespace MetaFunctions
 */
// ======================================================================
/*!
 * \namespace MetaFunctions
 * \brief Various functions related to meteorology
 *
 */
// ======================================================================

#ifndef METAFUNCTIONS_H
#define METAFUNCTIONS_H

#include "NFmiDataMatrix.h"
#include "NFmiFastQueryInfo.h"
#include <string>

namespace MetaFunctions
{
  bool isMeta(const std::string & theFunction);
  NFmiDataMatrix<float> values(const std::string & theFunction,
							   NFmiFastQueryInfo * theQI);

} // namespace MetaFunctions

#endif // METAFUNCTIONS_H

// ======================================================================
