import matplotlib.pyplot as plt
import numpy as np

data = [0.0000, 123.9877, 0.0000, 9870.9876]
x = np.sort(data)
arange = np.arange(len(data))
print(arange)
y = 1. * np.arange(len(data)) / (len(data) - 1)
print(y)
plt.plot(x, y)
# plt.show()
plt.savefig('cdf.png')


