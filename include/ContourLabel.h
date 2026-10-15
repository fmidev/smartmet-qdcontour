// ======================================================================
/*!
 * \file
 * \brief Interface of class ContourLabel
 */
// ======================================================================
/*!
 * \class ContourLabel
 * \brief Storage for a single contour-value
 *
 */
// ======================================================================

#ifndef CONTOURLABEL_H
#define CONTOURLABEL_H

#include <string>

class ContourLabel
{
 public:
#ifdef COMPILER_GENERATED
  ~ContourLabel();
  ContourLabel(const ContourLabel &theValue);
  ContourLabel &operator=(const ContourLabel &theValue);
#endif

  ContourLabel(float theValue);
  float value() const;

 private:
  ContourLabel() = delete;
  float itsValue;
};

#endif  // CONTOURLABEL_H

// ======================================================================
