#!/bin/sh
#
# pngdiff.sh <ok_png> <png> <diff_png>
#
# Compare produced PNG to a reference file, using ImageMagick.
#
# Ref: ImageMagick Comparing: 
#       <http://www.imagemagick.org/Usage/compare/>
#

OK_PNG=$1
PNG=$2
DIFF_PNG=$3

if [ \! -f $PNG ]; then
    echo "*** FAILED: 'results/${PNG}' not created"
    exit -1
fi

# Create a visual difference file
#
composite $OK_PNG $PNG -compose DIFFERENCE png:- | \
    convert - -contrast-stretch 0 $DIFF_PNG

# Get measurement of the difference
#
#   Metric can be:
#       AE (Absolute Error):    Number of pixels that were different, after applying fuzziness factor
#       PAE (Peak Absolute Error)
#       PSNR (Peak Signal/Noise Ratio): dB (1..inf) 1=all different, 20=differences 1/100 of maximum
#       MAE (Mean Absolute Error)
#       MSE (Mean Squared Error)
#       RMSE sqrt of MSE
#

# Note: '-metric AE' and '-fuzz n%' would be a good combo, but requires IM >= 6.4.3
#
#V="$(compare 2>&1 -metric AE -fuzz 5% ${OK_PNG} ${PNG} ${DIFF_PNG})"

# Note: IM 6.4.x outputs just the PSNR number (i.e. 22.432) but 6.2.8 outputs
#       "22.432 dB\n300,300,PNG" which is... troublesome
#
#V="$(compare 2>&1 -metric PSNR ${OK_PNG} ${PNG} ${DIFF_PNG} | head -1 | sed "-es/ dB//")"

V=$(compare 2>&1 -metric PSNR ${OK_PNG} ${PNG} ${DIFF_PNG} | head -1 | sed "-es/ dB//" || exit 255)

COL_RED="$(tput setaf 1)"
COL_GREEN="$(tput setaf 2)"
COL_YELLOW="$(tput setaf 3)"
COL_NORM="$(tput setaf 9)"

# Note: Need to use 'bc' to make non-integer value integer
#
if [ $(echo "$V >= 20" | bc) = 1 ]; then
    echo "${COL_GREEN}OK:${COL_NORM} PSNR >= 20dB ($V dB)"
    exit 0
elif [ $(echo "$V >= 10" | bc) = 1 ]; then
    echo "${COL_YELLOW}WARNING:${COL_NORM} 10dB <= PSNR < 20dB ($V dB)"
    exit 0
elif [ $(echo "$V >= 0" | bc) = 1 ]; then
    echo "${COL_RED}FAIL:${COL_NORM} PSNR < 10dB ($V dB)"
    exit 100
else
    echo "${COL_RED}FAIL:${COL_NORM} $V"
    exit 100
fi


#	convert -delay 50 results_ok/${PNG} results/${PNG} -loop 0 results_diff/$(PNG:.png=.anim.gif)

#	compare -metric AE -fuzz 50% results_ok/$(PNG) results/$(PNG) results_diff/$(PNG)

# This requires IM >= 6.4.2-8 (creates gray scale mask of the differences)
#
#	compare -metric AE -fuzz 5% results_ok/$(PNG) results/$(PNG) \
#	   -compose Src -highlight-color White -lowlight-color Black results_diff/$(PNG)

#	composite -compose DIFFERENCE results_ok/$(PNG) results/$(PNG) results_diff/$(PNG)
#	convert -threshold 25% -colorize 0/100/0 results_diff/$(PNG) results_diff/2_$(PNG)

#	compare -metric PSNR results_ok/$(PNG) results/$(PNG) results_diff/$(PNG)

