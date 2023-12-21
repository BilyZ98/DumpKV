import numpy as np
import matplotlib.pyplot as plt
import os
import sys
import bisect
import multiprocessing as mp

# y=ax^b

x = np.arange(0, 100, 1)
a = np.arange(0.1, 2, 0.2)
b = np.arange(0.1, 2, 0.2)  
for i in range(0, len(a)):
    y = a[i] * np.power(x, b[i])
    plt.plot(x, y, label='a={}, b={}'.format(a[i], b[i]))



plt.xlabel('x')
plt.ylabel('y')
plt.title('y=ax^b')
plt.legend()
plt.savefig('y=ax^b.png')
