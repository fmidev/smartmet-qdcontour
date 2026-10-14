// ======================================================================

#include "ContourInterpolation.h"

ContourInterpolation ContourInterpolationValue(const std::string &theName)
{
  if (theName == "Nearest")
    return Nearest;
  if (theName == "Linear")
    return Linear;
  if (theName == "LogLinear")
    return LogLinear;
  else if (theName == "Discrete")
    return Discrete;
  else
    return Missing;
}
