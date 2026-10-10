// ======================================================================
/*!
 * \file
 * \brief Interface of class ContourRange
 */
// ======================================================================
/*!
 * \class ContourRange
 * \brief Storage for a single contour range
 *
 */
// ======================================================================

#ifndef CONTOURRANGE_H
#define CONTOURRANGE_H

#include <string>

class ContourRange
{
 public:
#ifdef COMPILER_GENERATED
  ~ContourRange();
  ContourRange(const ContourRange &theValue);
  ContourRange &operator=(const ContourRange &theValue);
#endif

  ContourRange(float theLoLimit,
               float theHiLimit,
               int theColor,
               const std::string &theRule = "Atop");

  float lolimit() const;
  float hilimit() const;
  int color() const;
  const std::string& rule() const;

 private:
  ContourRange();

  float itsLoLimit;
  float itsHiLimit;
  int itsColor;
  std::string itsRule;

};  // class ContourRange

#endif  // CONTOURRANGE_H

// ======================================================================
