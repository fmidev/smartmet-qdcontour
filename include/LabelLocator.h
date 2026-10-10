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

  LabelLocator(const LabelLocator &theLocator);
  LabelLocator &operator=(const LabelLocator &theLocator);

  bool itHasBBox;
  int itsBBoxX1;
  int itsBBoxY1;
  int itsBBoxX2;
  int itsBBoxY2;

  float itsMinDistanceToSameValue;
  float itsMinDistanceToDifferentValue;
  float itsMinDistanceToDifferentParameter;

  int itsActiveParameter;
  ParamCoordinates itsPreviousCoordinates;
  ParamCoordinates itsCurrentCoordinates;

  // Private methods:

  bool inside(int theX1, int theY1) const;

  float distanceToBorder(float theX, float theY) const;

  void removeCandidates(ParamCoordinates &theCandidates,
                        const XY &thePoint,
                        int theParam,
                        float theContour);

  void removeEmpties(ParamCoordinates &theCandidates);

};  // class LabelLocator

#endif  // LABELLOCATOR_H

// ======================================================================
