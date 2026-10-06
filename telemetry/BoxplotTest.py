#! python3
import matplotlib.pyplot
import matplotlib.ticker
import numpy

dictColors = {0: 'dodgerblue', 1: 'orangered', 2: 'olivedrab', 3: 'goldenrod', 4: 'plum', 5: 'dimgray',
              6: 'skyblue', 7: 'orange', 8: 'yellowgreen', 9: 'gold', 10: 'thistle', 11: 'silver',
              12: 'aliceblue', 13: 'bisque', 14: 'mintcream', 15: 'lemonchiffon', 16: 'lavenderblush', 17: 'whitesmoke',
              18: 'midnightblue', 19: 'darkred', 20: 'darkgreen', 21: 'darkgoldenrod', 22: 'purple', 23: 'black'}

# 50 is boxplot outlier
dataOne = 50, 50, 55, 58, 63, 66, 66, 67, 67, 68, 69, 70, 70, 70, 70, 72, 73, 75, 75, 76, 76, 78, 79, 81

# 52 and 89 are boxplot outliers
dataTwo = 52, 57, 57, 58, 63, 66, 66, 67, 67, 68, 69, 70, 70, 70, 70, 72, 73, 75, 75, 76, 76, 78, 79, 89

testdata = 7.3, 8.2, 8.8, 8.9, 9.1, 9.2, 9.3, 9.5, 9.5, 9.7, 9.7, 9.9, 10.0, 10.3, 10.5, 10.8, 10.9, 11.2, 11.4, 12.0

"""
# Boxplots +   |-----|  |  |-----|  +
#          ^   ^  ^  ^  ^  ^  ^  ^  ^ Interquartile Range (IQR) = range of Q3 - Q1.
# Outlier --   |  |  |  |  |  |  |  | Points < Q1 - 1.5 * IQR.
# Minimum -----   |  |  |  |  |  |  | First data set point above Q1 - 1.5 * IQR.
# Whisker --------   |  |  |  |  |  |
# Q1 --------------  |  |  |  |  |  | 25th Quartile.
# Median -------------  |  |  |  |  | Q2/50th Quartile.
# Q3 ----------------------   |  |  | 75th Quartile.
# Whisker ------------------  |  |  |
# Maximum -----------------------   | First data set point below Q3 + 1.5 * IQR
# Outlier --------------------------  Point > Q3 + 1.5 * IQR
# Lets do the math for the data given:
dataQ1, dataQ2, dataQ3 = numpy.percentile(dataOne, [25, 50, 75])
IQR = dataQ3 - dataQ1
dataMin = dataQ1 - (1.5 * IQR)
dataMax = dataQ3 + (1.5 * IQR)
print(dataMin, dataQ1, dataQ2, dataQ3, dataMax, IQR)
dataQ1, dataQ2, dataQ3 = numpy.percentile(dataTwo, [25, 50, 75])
IQR = dataQ3 - dataQ1
dataMin = dataQ1 - (1.5 * IQR)
dataMax = dataQ3 + (1.5 * IQR)
print(dataMin, dataQ1, dataQ2, dataQ3, dataMax, IQR)
Wikipedia sets it to the minimum dataset value above
Minimum (Q1 - 1.5 * IQR) and maximum to the dataset value below Maximum (Q3 + 1.5 * IQR).

Relationship in Normal Distributions
If your dataset is normally distributed (bell-shaped), you can estimate quartiles using 
the mean (μ) and standard deviation (σ):
• First Quartile (Q₁): μ - 0.675σ (the 25th percentile)
• Third Quartile (Q₃): μ + 0.675σ (the 75th percentile)
• Interquartile Range (IQR): Q₃ - Q₁ ≈ 1.35σ

"""
graphfigure, axisleft = matplotlib.pyplot.subplots(figsize=(10, 7.5))

# axisleft.boxplot(
#     x=[dataOne, testdata],
#     tick_labels=['Dataset 1', 'Dataset 2'],
#     orientation='vertical',
#     notch=True
# )

print(numpy.average(dataOne))
print(numpy.std(dataOne))
# 1. Define your custom boxplot statistics in a dictionary
# You can manually change 'q1' to whatever value you need
stats_one = [{
    'label': 'stats one',
    'med': numpy.average(dataOne),
    'q1': numpy.average(dataOne) - numpy.std(dataOne),
    'q3': numpy.average(dataOne) + numpy.std(dataOne),
    'whislo': numpy.average(dataOne) - numpy.std(dataOne) - 5,
    'whishi': numpy.average(dataOne) + numpy.std(dataOne) + 10,
    'fliers': []
}]

stats_two = [{
    'label': 'stats two',
    'med': numpy.average(dataOne),
    'q1': numpy.average(dataOne) - numpy.std(dataOne),
    'q3': numpy.average(dataOne) + numpy.std(dataOne),
    'whislo': numpy.average(dataOne) - numpy.std(dataOne) - 10,
    'whishi': numpy.average(dataOne) + numpy.std(dataOne) + 10,
    'fliers': []
}]

all_stats = stats_one + stats_two
positons = [1, 2]

axisleft.bxp(all_stats, positons)

text = " - foo"
axisleft.set_title('Boxplot{}'.format(text))
axisleft.set_xlabel('Ordered Data Values')

matplotlib.pyplot.show()
