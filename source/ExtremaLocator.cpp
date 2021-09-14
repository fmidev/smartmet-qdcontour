// ======================================================================
/*!
 * \file
 * \brief Implementation of class ExtremaLocator
 */
// ======================================================================
/*!
 * \class ExtremaLocator
 *
 * \brief Finds suitable locations for extrema markers
 *
 * The ExtremaLocator class provides means for finding suitable locations
 * for extrema markers based on
 *
 *   - the bounding box for the coordinates
 *   - the previous timestep locations
 *   - the minimum allowed distance to nearest extrema of same type
 *   - the minimum allowed distance to nearest extrema of different type
 *
 * Note that all the relevant settings should be made before adding
 * any candidate coordinates to the object. In particular, the bounding
 * box filtering is used to immediately discard coordinates outside
 * the target image. The code enforces this by throwing an exception
 * when the settings are changed while coordinates have been added.
 *
 * The algorithm for choosing the coordinates for a new timestep is
 *
 *   -# discard all points not within the bounding box
 *   -# while there are candidate coordinates
 *    -# loop over parameters
 *     -# loop over extrema types
 *      -# select label closest to a label from an earlier timestep
 *      -# loop over all remaining candidates, removing too close ones
 *
 * The algorithm for choosing the label positions for the first
 * timestep \b when a bounding box has been specified is the same,
 * but the candidate coordinates are sorted based on their distances
 * to the bounding box instead of the previous timestep. Points
 * closest to the bounding box are preferred.
 *
 * If there is no bounding box, we simply choose the first one
 * available.
 */
// ======================================================================

#include "ExtremaLocator.h"

#include <cmath>
#include <stdexcept>

using namespace std;

//! Bad parameter value

const int badparameter = 0;

namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Distance between two points
 */
// ----------------------------------------------------------------------

double distance(double theX1, double theY1, double theX2, double theY2)
{
  return sqrt((theX2 - theX1) * (theX2 - theX1) + (theY2 - theY1) * (theY2 - theY1));
}

// ----------------------------------------------------------------------
/*!
 * \brief Minimum distance of point from collection of points
 */
// ----------------------------------------------------------------------

template <typename T>
double mindistance(double theX, double theY, const T &theCoords)
{
  double best = -1;
  for (typename T::const_iterator it = theCoords.begin(); it != theCoords.end(); ++it)
  {
    double dist = distance(theX, theY, it->first, it->second);
    if (best < 0)
      best = dist;
    else
      best = min(best, dist);
  }
  return best;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

ExtremaLocator::~ExtremaLocator() {}
// ----------------------------------------------------------------------
/*!
 * \brief Default constructor
 */
// ----------------------------------------------------------------------

ExtremaLocator::ExtremaLocator()
    : itsMinDistanceToSame(500),
      itsMinDistanceToDifferent(500),
      itsPreviousCoordinates(),
      itsCurrentCoordinates()
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the locator contains no coordinates
 *
 * \return True, if there are no coordinates in the locator
 */
// ----------------------------------------------------------------------

bool ExtremaLocator::empty() const
{
  return (itsPreviousCoordinates.empty() && itsCurrentCoordinates.empty());
}

// ----------------------------------------------------------------------
/*!
 * \brief Clear all earlier label locations
 */
// ----------------------------------------------------------------------

void ExtremaLocator::clear()
{
  itsPreviousCoordinates.clear();
  itsCurrentCoordinates.clear();
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the minimum distance to label of same value
 *
 * \param theDistance The minimum distance
 */
// ----------------------------------------------------------------------

void ExtremaLocator::minDistanceToSame(float theDistance)
{
  if (!empty() && itsMinDistanceToSame != theDistance)
    throw runtime_error(
        "ExtremaLocator: Cannot change minimum distances once "
        "coordinates have been added");

  itsMinDistanceToSame = theDistance;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the minimum distance to label of different value
 *
 * \param theDistance The minimum distance
 */
// ----------------------------------------------------------------------

void ExtremaLocator::minDistanceToDifferent(float theDistance)
{
  if (!empty() && itsMinDistanceToDifferent != theDistance)
    throw runtime_error(
        "ExtremaLocator: Cannot change minimum distances once "
        "coordinates have been added");

  itsMinDistanceToDifferent = theDistance;
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize next time step
 *
 * This must be called before coordinates are added for any new timestep.
 * It should be called also before the first timestep, although at this
 * point it could be omitted. This is not however guaranteed in the
 * future.
 */
// ----------------------------------------------------------------------

void ExtremaLocator::nextTime()
{
  itsPreviousCoordinates.clear();
  swap(itsPreviousCoordinates, itsCurrentCoordinates);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a new coordinate
 *
 * \param theContour The contour value
 * \param theX The X-coordinate
 * \param theY The Y-coordinate
 */
// ----------------------------------------------------------------------

void ExtremaLocator::add(Extremum theType, double theX, double theY)
{
  // Default constructed values are a desired side-effect in here
  // This is much simpler than using find + insert with checking

  Coordinates &c = itsCurrentCoordinates[theType];

  c.push_back(Coordinates::value_type(theX, theY));
}

// ----------------------------------------------------------------------
/*!
 * \brief Choose the final label locations
 */
// ----------------------------------------------------------------------

const ExtremaLocator::ExtremaCoordinates &ExtremaLocator::chooseCoordinates()
{
  // Make a duplicate for the final choices

  ExtremaCoordinates candidates;
  ExtremaCoordinates choices;
  swap(itsCurrentCoordinates, candidates);

  while (!candidates.empty())
  {
    for (ExtremaCoordinates::iterator cit = candidates.begin(); cit != candidates.end(); ++cit)
    {
      // removeCandidates may have cleared some contour from
      // possible coordinates

      if (cit->second.empty()) continue;

      // find the best label coordinate

      const Extremum value = cit->first;

      Coordinates::const_iterator best = chooseOne(cit->second, value);
      if (best == cit->second.end())
        throw runtime_error("Internal error in ExtremaLocator::chooseLabels()");

      // add the best label coordinate

      Coordinates &coords = choices[value];
      coords.push_back(*best);

      // and erase all candidates too close to the accepted coordinate

      removeCandidates(candidates, *best, value);
    }

    // Now we erase any possible empty containers left behind

    removeEmpties(candidates);
  }

  swap(itsCurrentCoordinates, choices);

  return itsCurrentCoordinates;
}

// ----------------------------------------------------------------------
/*!
 * \brief Choose a label location from given candidates
 *
 * \param theCandidates The list of candidates
 * \param theType The extremum type
 */
// ----------------------------------------------------------------------

ExtremaLocator::Coordinates::const_iterator ExtremaLocator::chooseOne(
    const Coordinates &theCandidates, Extremum theType)
{
  ExtremaCoordinates::const_iterator pit = itsPreviousCoordinates.find(theType);
  if (pit == itsPreviousCoordinates.end()) return chooseClosestToBorder(theCandidates, theType);

  return chooseClosestToPrevious(theCandidates, pit->second, theType);
}

// ----------------------------------------------------------------------
/*!
 * \brief Choose a label location from given candidates closest to earlier ones
 *
 * \param theCandidates The list of candidates
 * \param thePreviousChoises The list of choises from previous timestep
 * \param theType The extremum type
 */
// ----------------------------------------------------------------------

ExtremaLocator::Coordinates::const_iterator ExtremaLocator::chooseClosestToPrevious(
    const Coordinates &theCandidates, const Coordinates &thePreviousChoises, Extremum theType)
{
  double bestdist = -1;
  Coordinates::const_iterator best = theCandidates.end();

  for (Coordinates::const_iterator it = theCandidates.begin(); it != theCandidates.end(); ++it)
  {
    double mindist = mindistance(it->first, it->second, thePreviousChoises);
    if (bestdist < 0 || mindist < bestdist)
    {
      bestdist = mindist;
      best = it;
    }
  }
  return best;
}

// ----------------------------------------------------------------------
/*!
 * \brief Choose a label location closest to the bounding box
 *
 * \param theCandidates The list of candidates
 * \param theType The extremum type
 */
// ----------------------------------------------------------------------

ExtremaLocator::Coordinates::const_iterator ExtremaLocator::chooseClosestToBorder(
    const Coordinates &theCandidates, Extremum theType)
{
  if (theCandidates.empty()) return theCandidates.end();

  return theCandidates.begin();
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove candidates too close to the chosen point
 *
 * Note that the code erases only candidate coordinates and may
 * leave empty containers behind. This is necessary so that
 * any top level iterators will not become invalidated.
 *
 * \param theCandidates The candidates to clean up
 * \param thePoint The chosen point
 * \param theParam The chosen parameter
 * \param theContour The chosen contour value
 */
// ----------------------------------------------------------------------

void ExtremaLocator::removeCandidates(ExtremaCoordinates &theCandidates,
                                      const XY &thePoint,
                                      Extremum theType)
{
  for (ExtremaCoordinates::iterator pit = theCandidates.begin(); pit != theCandidates.end();)
  {
    const Extremum etype = pit->first;

    for (Coordinates::iterator it = pit->second.begin(); it != pit->second.end();)
    {
      const double dist = distance(thePoint.first, thePoint.second, it->first, it->second);

      bool erase = false;

      if (etype != theType)
        erase = (dist < itsMinDistanceToDifferent);
      else
        erase = (dist < itsMinDistanceToSame);

      if (erase)
        it = pit->second.erase(it);
      else
        ++it;
    }
    ++pit;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove any empty subcontainers from the candidates
 *
 * The details of the algorithm are important. Note how empty elements
 * are deleted so that the current iterator will not be invalidated.
 *
 */
// ----------------------------------------------------------------------

void ExtremaLocator::removeEmpties(ExtremaCoordinates &theCandidates)
{
  for (ExtremaCoordinates::iterator pit = theCandidates.begin(); pit != theCandidates.end();)
  {
    if (pit->second.empty())
      theCandidates.erase(pit++);
    else
      ++pit;
  }
}

// ======================================================================
