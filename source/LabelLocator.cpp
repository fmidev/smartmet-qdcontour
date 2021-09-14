// ======================================================================
/*!
 * \file
 * \brief Implementation of class LabelLocator
 */
// ======================================================================
/*!
 * \class LabelLocator
 *
 * \brief Finds suitable locations for contour labels
 *
 * The LabelLocator class provides means for finding suitable locations
 * for contour labels based on
 *
 *   - the bounding box for the coordinates
 *   - the previous timestep locations
 *   - the minimum allowed distance to nearest same value label
 *   - the minimum allowed distance to nearest different value label
 *   - the minimum allowed distance to nearest label of another parameter
 *
 * Note that all the relevant settings should be made before adding
 * any candidate coordinates to the object. In particular, the bounding
 * box filtering is used to immediately discard coordinates outside
 * the target image. The code enforces this by throwing an exception
 * when the settings are changed while coordinates have been added.
 *
 * The algorithm for choosing the labels for a new timestep is
 *
 *   -# discard all points not within the bounding box
 *   -# while there are candidate coordinates
 *    -# loop over parameters
 *     -# loop over contour values
 *      -# select label closest to a label from an earlier timestep
 *      -# loop over all contours of all different parameters
 *        -# remove candidates too close to the chosen one
 *      -# loop over all different contours of same parameter
 *        -# remove candidates too close to the chosen one
 *      -# loop over all candidates for the same contour
 *        -# remove candidates too close to the chosen one
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

#include "LabelLocator.h"

#include <cmath>
#include <iostream>
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
    double dist = distance(theX, theY, it->second.first, it->second.second);
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

LabelLocator::~LabelLocator() {}
// ----------------------------------------------------------------------
/*!
 * \brief Default constructor
 */
// ----------------------------------------------------------------------

LabelLocator::LabelLocator()
    : itHasBBox(false),
      itsBBoxX1(0),
      itsBBoxY1(0),
      itsBBoxX2(0),
      itsBBoxY2(0),
      itsMinDistanceToSameValue(175),
      itsMinDistanceToDifferentValue(30),
      itsMinDistanceToDifferentParameter(30),
      itsActiveParameter(0),
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

bool LabelLocator::empty() const
{
  return (itsPreviousCoordinates.empty() && itsCurrentCoordinates.empty());
}

// ----------------------------------------------------------------------
/*!
 * \brief Clear all earlier label locations
 */
// ----------------------------------------------------------------------

void LabelLocator::clear()
{
  itsActiveParameter = badparameter;
  itsPreviousCoordinates.clear();
  itsCurrentCoordinates.clear();
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the bounding box to which all label coordinates are clipped
 *
 * Note that normally the bounding box does not correspond directly to
 * the image dimensions, but one substracts some margin width first
 * to prevent labels from being place too close to the borders where
 * the label text might be rendered outside the image.
 *
 * \param theX1 The minimum X-coordinate
 * \param theY1 The minimum Y-coordinate
 * \param theX2 The maximum X-coordinate + 1
 * \param theY2 The maximum Y-coordinate + 1
 */
// ----------------------------------------------------------------------

void LabelLocator::boundingBox(int theX1, int theY1, int theX2, int theY2)
{
  if (theX2 <= theX1 || theY2 <= theY1)
    throw runtime_error("Empty bounding box not allowed in LabelLocator");

  if (!empty())
  {
    if (!itHasBBox || itsBBoxX1 != theX1 || itsBBoxY1 != theY1 || itsBBoxX2 != theX2 ||
        itsBBoxY2 != theY2)
      throw runtime_error(
          "LabelLocator: Cannot change bounding box once "
          "coordinates have been added");
  }

  itHasBBox = true;
  itsBBoxX1 = theX1;
  itsBBoxY1 = theY1;
  itsBBoxX2 = theX2;
  itsBBoxY2 = theY2;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the minimum distance to label of same value
 *
 * \param theDistance The minimum distance
 */
// ----------------------------------------------------------------------

void LabelLocator::minDistanceToSameValue(float theDistance)
{
  if (!empty() && itsMinDistanceToSameValue != theDistance)
    throw runtime_error(
        "LabelLocator: Cannot change minimum distances once "
        "coordinates have been added");

  itsMinDistanceToSameValue = theDistance;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the minimum distance to label of different value
 *
 * \param theDistance The minimum distance
 */
// ----------------------------------------------------------------------

void LabelLocator::minDistanceToDifferentValue(float theDistance)
{
  if (!empty() && itsMinDistanceToDifferentValue != theDistance)
    throw runtime_error(
        "LabelLocator: Cannot change minimum distances once "
        "coordinates have been added");

  itsMinDistanceToDifferentValue = theDistance;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the minimum distance to label of parameter
 *
 * \param theDistance The minimum distance
 */
// ----------------------------------------------------------------------

void LabelLocator::minDistanceToDifferentParameter(float theDistance)
{
  if (!empty() && itsMinDistanceToDifferentParameter != theDistance)
    throw runtime_error(
        "LabelLocator: Cannot change minimum distances once "
        "coordinates have been added");

  itsMinDistanceToDifferentParameter = theDistance;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the parameter id of the active parameter
 *
 * Parameter number 0 is reserved for indicating no active parameter.
 * This co-incides with the newbase value kFmiBadParameter. However,
 * one should not use this method to set parameter value 0, instead
 * one should use the clear method.
 *
 * \param theParameter The parameter value
 */
// ----------------------------------------------------------------------

void LabelLocator::parameter(int theParameter)
{
  if (theParameter == badparameter)
    throw runtime_error("Cannot activate parameter number 0, must be nonnegative");

  itsActiveParameter = theParameter;
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

void LabelLocator::nextTime()
{
  itsPreviousCoordinates.clear();
  swap(itsPreviousCoordinates, itsCurrentCoordinates);
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the point is within the bounding box
 *
 * Note that the bounding box may not be set, in which case all
 * points are considered inside.
 *
 * \param theX The X-coordinate
 * \param theY The Y-coordinate
 * \return True, if the point is inside the bounding box
 */
// ----------------------------------------------------------------------

bool LabelLocator::inside(int theX, int theY) const
{
  if (!itHasBBox) return true;

  return (theX >= itsBBoxX1 && theX < itsBBoxX2 && theY >= itsBBoxY1 && theY < itsBBoxY2);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a new coordinate for the active parameter
 *
 * \param theContour The contour value
 * \param theX The X-coordinate
 * \param theY The Y-coordinate
 */
// ----------------------------------------------------------------------

void LabelLocator::add(float theContour, int theX, int theY)
{
  // Ignore the point if it is not within the bounding box

  if (!inside(theX, theY)) return;

  // Cannot add any coordinates for non-existent parameter

  if (itsActiveParameter == badparameter)
    throw runtime_error("LabelLocator: Cannot add label location before setting the parameter");

  // Default constructed values are a desired side-effect in here
  // This is much simpler than using find + insert with checking

  ContourCoordinates &cc = itsCurrentCoordinates[itsActiveParameter];
  Coordinates &c = cc[theContour];

  // Now calculate the distance value used for sorting

  ParamCoordinates::const_iterator it = itsPreviousCoordinates.find(itsActiveParameter);

  float dist;
  if (it == itsPreviousCoordinates.end())
    dist = distanceToBorder(static_cast<float>(theX), static_cast<float>(theY));
  else
  {
    ContourCoordinates::const_iterator jt = it->second.find(theContour);
    if (jt == it->second.end())
      dist = distanceToBorder(static_cast<float>(theX), static_cast<float>(theY));
    else
      dist = static_cast<float>(mindistance(static_cast<float>(theX), theY, jt->second));
  }

  c.insert(Coordinates::value_type(dist, XY(theX, theY)));
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate point distance to border
 *
 * \param theX The X-coordinate
 * \param theY The Y-coordinate
 * \return The distance
 */
// ----------------------------------------------------------------------

float LabelLocator::distanceToBorder(float theX, float theY) const
{
  if (!itHasBBox) return 0;

  double xdist =
      min(abs(theX - static_cast<float>(itsBBoxX1)), abs(theX - static_cast<float>(itsBBoxX2)));
  double ydist =
      min(abs(theY - static_cast<float>(itsBBoxY1)), abs(theY - static_cast<float>(itsBBoxY2)));
  return static_cast<float>(min(xdist, ydist));
}

// ----------------------------------------------------------------------
/*!
 * \brief Choose the final label locations
 */
// ----------------------------------------------------------------------

const LabelLocator::ParamCoordinates &LabelLocator::chooseLabels()
{
  // Make a duplicate for the final choices

  ParamCoordinates candidates;
  ParamCoordinates choices;
  swap(itsCurrentCoordinates, candidates);

  while (!candidates.empty())
  {
    const int param = candidates.begin()->first;
    ContourCoordinates &contours = candidates.begin()->second;

    for (ContourCoordinates::iterator cit = contours.begin(); cit != contours.end(); ++cit)
    {
      // removeCandidates may have cleared some contour from
      // possible coordinates

      if (cit->second.empty()) continue;

      // the candidate is the first label coordinate, since we've
      // sorted them so

      if (cit->second.empty())
        throw runtime_error("Internal error in LabelLocator::chooseLabels()");
      const float value = cit->first;
      const Coordinates::value_type best = *(cit->second.begin());

#if 0
		  cout << "Chose: " << best.first << " at "
			   << best.second.first
			   << ' '
			   << best.second.second
			   << endl;
#endif

      // add the best label coordinate

      ContourCoordinates &contours = choices[param];
      Coordinates &coords = contours[value];
      coords.insert(best);

      // and erase all candidates too close to the accepted coordinate

      removeCandidates(candidates, best.second, param, value);
    }

    // Now we erase any possible empty containers left behind

    removeEmpties(candidates);
  }

  swap(itsCurrentCoordinates, choices);

  return itsCurrentCoordinates;
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

void LabelLocator::removeCandidates(ParamCoordinates &theCandidates,
                                    const XY &thePoint,
                                    int theParam,
                                    float theContour)
{
  for (ParamCoordinates::iterator pit = theCandidates.begin(); pit != theCandidates.end();)
  {
    const int param = pit->first;
    for (ContourCoordinates::iterator cit = pit->second.begin(); cit != pit->second.end();)
    {
      const float contour = cit->first;
      for (Coordinates::iterator it = cit->second.begin(); it != cit->second.end();)
      {
        const double dist =
            distance(thePoint.first, thePoint.second, it->second.first, it->second.second);

        bool erase = false;

        if (param != theParam)
          erase = (dist < itsMinDistanceToDifferentParameter);
        else if (contour != theContour)
          erase = (dist < itsMinDistanceToDifferentValue);
        else
          erase = (dist < itsMinDistanceToSameValue);

        if (erase)
          cit->second.erase(it++);
        else
          ++it;
      }
      ++cit;
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

void LabelLocator::removeEmpties(ParamCoordinates &theCandidates)
{
  for (ParamCoordinates::iterator pit = theCandidates.begin(); pit != theCandidates.end();)
  {
    for (ContourCoordinates::iterator cit = pit->second.begin(); cit != pit->second.end();)
    {
      if (cit->second.empty())
        pit->second.erase(cit++);
      else
        ++cit;
    }
    if (pit->second.empty())
      theCandidates.erase(pit++);
    else
      ++pit;
  }
}

// ======================================================================
