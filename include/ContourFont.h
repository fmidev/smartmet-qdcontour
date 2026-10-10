// ======================================================================
/*!
 * \file
 * \brief Interface of class ContourFont
 */
// ======================================================================
/*!
 * \class ContourFont
 * \brief Storage for a single contour symbol
 *
 */
// ======================================================================

#ifndef CONTOURFONT_H
#define CONTOURFONT_H

#include <string>

class ContourFont
{
 public:
#ifdef COMPILER_GENERATED
  ContourFont(const ContourFont &theValue);
  ContourFont &operator=(const ContourFont &theValue);
#endif

  virtual ~ContourFont() {}
  ContourFont(float theValue, int theColor, int theSymbol, const std::string &theFont);

  float value() const;
  int color() const;
  int symbol() const;
  const std::string &font() const;

 private:
  ContourFont();

  float itsValue;
  int itsColor;
  int itsSymbol;
  std::string itsFont;
};

#endif  // CONTOURFONT_H

// ======================================================================
