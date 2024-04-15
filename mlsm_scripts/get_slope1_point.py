
import numpy as np
from scipy.optimize import fsolve
import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

file_path = sys.argv[1]

def find_slope_one(cdf_array):
    # Calculate the derivative of the CDF
    # derivative = np.gradient(cdf_array)
    # max_value = np.max(cdf_array)
    cdf_arr = np.sort(cdf_array)  
    cdf_arr_y = np.arange(len(cdf_arr)) 
    # cdf_arr_y = 1.0 * np.arange(len(cdf_arr)) / (len(cdf_arr) - 1)

    # x_count = {}
    # for i in cdf_array:
    #     if i in x_count:
    #         x_count[i] += 1
    #     else:
    #         x_count[i] = 1

    # cdf_x = list(x_count.keys())
    # cdf_x.sort()
    # cdf_y = []
    # for i in cdf_x:
    #     cdf_y.append(x_count[i])
    # cdf_y = np.cumsum(cdf_y)


    # fit a curve to the data using a 3rd degree polynomial
    p = np.polyfit(cdf_arr, cdf_arr_y, 3)
    # print("The polynomial coefficients are:", p)
    # for i in range(len(p)):
    #     print(p[i])

    # get the derivative of the polynomial
    p_deriv = np.polyder(p)
    # print("The derivative of the polynomial is:", p_deriv)
    # for i in range(len(p_deriv)):
    #     print(p_deriv[i])

    # define a function where the derivative equals 1
    def f(x):
        return np.polyval(p_deriv, x) - 1

    # use a solver to find the x where the derivative equals 1
    x_initial_guess = 1 * 1024 * 1024 
    x_solution = fsolve(f, x_initial_guess)

    # output format [123]
    # get rid of []
    x_solution = x_solution[0]
    return x_solution  


if __name__ == '__main__':
    cdf_arr = pd.read_csv(file_path, header=0).values.flatten()
    slope_one = find_slope_one(cdf_arr)
    print(slope_one)

