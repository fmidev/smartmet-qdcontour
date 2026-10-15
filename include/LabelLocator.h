// ======================================================================
/*!
 * \file
 * \brief Interface of class LabelLocator
 */
// ======================================================================

#ifndef LABELLOCATOR_H
#define LABELLOCATOR_H

#include <list>
#include <map>

class LabelLocator
{
 public:
  ~LabelLocator();
  LabelLocator();

  bool empty() const;
  void clear();

  void boundingBox(int theX1, int theY1, int theX2, int theY2);
  void minDistanceToSameValue(float theDistance);
  void minDistanceToDifferentValue(float theDistance);
  void minDistanceToDifferentParameter(float theDistance);

  void parameter(int theParameter);
  void nextTime();

  void add(float theContour, int theX, int theY);

  using XY = std::pair<int, int>;
  using Coordinates = std::multimap<float, XY>;
  using ContourCoordinates = std::map<float, Coordinates>;
  using ParamCoordinates = std::map<int, ContourCoordinates>;

  const ParamCoordinates &chooseLabels();

 private:
  // Intentionally disabled:

  LabelLocator(const LabelLocator& theLocator) = delete;
  LabelLocator& operator=(const LabelLocator& theLocator) = delete;

  bool itHasBBox = false;
  int itsBBoxX1 = 0;
  int itsBBoxY1 = 0;
  int itsBBoxX2 = 0;
  int itsBBoxY2 = 0;

  float itsMinDistanceToSameValue = 175;
  float itsMinDistanceToDifferentValue = 30;
  float itsMinDistanceToDifferentParameter = 30;

  int itsActiveParameter = 0;
  ParamCoordinates itsPreviousCoordinates;
  ParamCoordinates itsCurrentCoordinates;

  // Private methods:

  bool inside(int theX1, int theY1) const;

  float distanceToBorder(float theX, float theY) const;

  void removeCandidates(ParamCoordinates &theCandidates,
                        const XY &thePoint,
                        int theParam,
                        float theContour) const;

  void removeEmpties(ParamCoordinates &theCandidates);

};  // class LabelLocator

#endif  // LABELLOCATOR_H

// ======================================================================
