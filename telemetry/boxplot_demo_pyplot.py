"""
============
Boxplot Demo
============

Example boxplot code
"""

import numpy
import matplotlib.pyplot

# Fixing random state for reproducibility
numpy.random.seed(19680801)

# fake up some data
spread = numpy.random.rand(50) * 100
center = numpy.ones(25) * 50
flier_high = numpy.random.rand(10) * 100 + 100
flier_low = numpy.random.rand(10) * -100
data = numpy.concatenate((spread, center, flier_high, flier_low))

###############################################################################

fig1, ax1 = matplotlib.pyplot.subplots()
ax1.set_title('Basic Plot')
ax1.boxplot(data)

###############################################################################

fig2, ax2 = matplotlib.pyplot.subplots()
ax2.set_title('Notched boxes')
ax2.boxplot(data, notch=True)

###############################################################################

green_diamond = dict(markerfacecolor='g', marker='D')
fig3, ax3 = matplotlib.pyplot.subplots()
ax3.set_title('Changed Outlier Symbols')
ax3.boxplot(data, flierprops=green_diamond)

###############################################################################

fig4, ax4 = matplotlib.pyplot.subplots()
ax4.set_title('Hide Outlier Points')
ax4.boxplot(data, showfliers=False)

###############################################################################

red_square = dict(markerfacecolor='r', marker='s')
fig5, ax5 = matplotlib.pyplot.subplots()
ax5.set_title('Horizontal Boxes')
ax5.boxplot(data, vert=False, flierprops=red_square)

###############################################################################

fig6, ax6 = matplotlib.pyplot.subplots()
ax6.set_title('Shorter Whisker Length')
ax6.boxplot(data, flierprops=red_square, vert=False, whis=0.75)

###############################################################################
# Fake up some more data

spread = numpy.random.rand(50) * 100
center = numpy.ones(25) * 40
flier_high = numpy.random.rand(10) * 100 + 100
flier_low = numpy.random.rand(10) * -100
d2 = numpy.concatenate((spread, center, flier_high, flier_low))
data.shape = (-1, 1)
d2.shape = (-1, 1)

###############################################################################
# Making a 2-D array only works if all the columns are the
# same length.  If they are not, then use a list instead.
# This is actually more efficient because boxplot converts
# a 2-D array into a list of vectors internally anyway.

# data = [data, d2, d2[::2, 0]]
# fig7, ax7 = matplotlib.pyplot.subplots()
# ax7.set_title('Multiple Samples with Different sizes')
# ax7.boxplot(data)

#############################################################################

matplotlib.pyplot.show()
