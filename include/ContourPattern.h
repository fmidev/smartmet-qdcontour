// ======================================================================
/*!
 * \file
 * \brief Interface of class ContourPattern
 */
// ======================================================================
/*!
 * \class ContourPattern
 * \brief Storage for a single contour pattern
 *
 */
// ======================================================================

#ifndef CONTOURPATTERN_H
#define CONTOURPATTERN_H

#include <string>

class ContourPattern
{
 public:
#ifdef COMPILER_GENERATED
  ContourPattern(const ContourPattern &theValue);
  ContourPattern &operator=(const ContourPattern &theValue);
#endif

  virtual ~ContourPattern() = default;
  ContourPattern(float theLoLimit,
                 float theHiLimit,
                 const std::string &thePattern,
                 const std::string &theRule,
                 float theFactor = 1.0);

  float lolimit() const;
  float hilimit() const;
  const std::string& pattern() const;
  const std::string& rule() const;
  float factor() const;

 private:
  ContourPattern();

  float itsLoLimit;
  float itsHiLimit;
  std::string itsPattern;
  std::string itsRule;
  float itsFactor;

};  // class ContourPattern

#endif  // CONTOURPATTERN_H

// ======================================================================
