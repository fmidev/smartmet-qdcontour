// ======================================================================
/*!
 * \file
 * \brief Interface of class ContourValue
 */
// ======================================================================
/*!
 * \class ContourValue
 * \brief Storage for a single contour-value
 *
 */
// ======================================================================

#ifndef CONTOURVALUE_H
#define CONTOURVALUE_H

#include <string>

class ContourValue
{
 public:
#ifdef COMPILER_GENERATED
  ~ContourValue();
  ContourValue(const ContourValue &theValue);
  ContourValue &operator=(const ContourValue &theValue);
#endif

  ContourValue(float theValue,
               float theLineWidth,
               int theColor,
               const std::string &theRule = "Atop");

  float linewidth() const;
  float value() const;
  int color() const;
  const std::string &rule() const;

 private:
  ContourValue();

  float itsValue;
  float itsLineWidth;
  int itsColor;
  std::string itsRule;
};

#endif  // CONTOURVALUE_H

// ======================================================================
