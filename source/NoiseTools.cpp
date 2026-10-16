// ======================================================================
/*!
 * \brief Implementation of namespace NoiseTools
 */
// ======================================================================

#include "NoiseTools.h"
#include <algorithm>
#include <iterator>

namespace NoiseTools
{
namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Test whether the value is to be filtered: not missing and within the limits
 */
// ----------------------------------------------------------------------

bool is_filtered(float value, float theLoLimit, float theHiLimit)
{
  if (value == kFloatMissing) return false;
  if (theLoLimit != kFloatMissing && value < theLoLimit) return false;
  if (theHiLimit != kFloatMissing && value > theHiLimit) return false;
  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Append the non-missing values within the radius of (i,j) to the list
 */
// ----------------------------------------------------------------------

void collect_neighbours(const NFmiDataMatrix<float> &theValues,
                        size_t i,
                        size_t j,
                        size_t theRadius,
                        std::vector<float> &theList)
{
  for (size_t jj = j - std::min(j, theRadius); jj < std::min(theValues.NY(), j + theRadius + 1);
       ++jj)
    for (size_t ii = i - std::min(i, theRadius); ii < std::min(theValues.NX(), i + theRadius + 1);
         ++ii)
    {
      const float value = theValues[ii][jj];
      if (value != kFloatMissing) theList.push_back(value);
    }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Weighted median filter
 *
 * \param theLoLimit Lolimit for data to filter (or kFloatMissing)
 * \param theHiLimit Hilimit for data to filter (or kFloatMissing)
 * \param theRadius The median filter radius
 * \param theWeight The median filter weight
 */
// ----------------------------------------------------------------------

void despeckle(NFmiDataMatrix<float> &theValues,
               float theLoLimit,
               float theHiLimit,
               size_t theRadius,
               float theWeight)
{
  NFmiDataMatrix<float> oldvalues(theValues);

  // Preallocate the vector for speed
  std::vector<float> medianlist;
  medianlist.reserve((2 * theRadius + 1) * (2 * theRadius + 1));

  for (size_t j = 0; j < theValues.NY(); j++)
    for (size_t i = 0; i < theValues.NX(); i++)
    {
      medianlist.clear();

      // Do not filter the pixel if the value is missing
      // or the value is not in the desired range

      const float oldvalue = oldvalues[i][j];
      if (!is_filtered(oldvalue, theLoLimit, theHiLimit)) continue;

      // Do median filtering

      collect_neighbours(oldvalues, i, j, theRadius, medianlist);

      if (!medianlist.empty())
      {
        int pos = static_cast<int>(
            round((static_cast<float>(medianlist.size()) - 1) * theWeight / 100.0));
        std::nth_element(medianlist.begin(), medianlist.begin() + pos, medianlist.end());

#if 0
			  if(FmiRound(medianlist[pos] + 0.1 > 0.01))
				{
				  std::cout << "Coordinates: " << i << " " << j << std::endl;
				  std::cout << "Old value: " << oldvalues[i][j] << std::endl;
				  
				  std::copy(medianlist.begin(),
							medianlist.end(),
							std::ostream_iterator<float>(std::cout," "));
				  
				  std::cout << " --> " << medianlist[pos] << std::endl << std::endl;
				}
#endif

        theValues[i][j] = medianlist[pos];
      }
    }
}

// ----------------------------------------------------------------------
/*!
 * \brief Recursive weighted median filter
 *
 * \param theLoLimit Lolimit for data to filter (or kFloatMissing)
 * \param theHiLimit Hilimit for data to filter (or kFloatMissing)
 * \param theRadius The median filter radius
 * \param theWeight The median filter weight
 * \param theIterations The number of iterations
 */
// ----------------------------------------------------------------------

void despeckle(NFmiDataMatrix<float> &theValues,
               float theLoLimit,
               float theHiLimit,
               int theRadius,
               float theWeight,
               int theIterations)
{
  // Quick exits for trivial cases
  if (theRadius < 1 || theIterations < 1) return;

  for (int iter = 0; iter < theIterations; ++iter)
    despeckle(theValues, theLoLimit, theHiLimit, theRadius, theWeight);
}

}  // namespace NoiseTools

// ======================================================================
