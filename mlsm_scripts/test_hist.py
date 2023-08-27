import matplotlib.pyplot as plt

# Create a list of data
data = [2, 2, 3, 4, 5, 6, 7, 8, 9, 10]

# Plot histogram
plt.hist(data, rwidth=0.8)
# plt.show()
plt.savefig('hist.png' )
