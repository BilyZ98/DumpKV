
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import bisect
import os
import sys
import multiprocessing as mp


data_path = "/home/zt/ycsb-workload-gen/data/workloada-run-10000000-10000000.log.formated"


def GetWriteCDFForEachKey():
    # nothing to do
    pass
df = pd.read_csv(data_path, delimiter=' ', header=None, names=['op', 'key'])
df_count_update = df.groupby('op').get_group('UPDATE')
df_count = df_count_update.groupby('key').size().reset_index(name='counts')
df_count_sort = df_count.sort_values(by=['counts'], ascending=True)
count_row, count_col = df_count_sort.shape
cdf_x = np.array(df_count_sort['counts'])
cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df), len(df_count)))
plt.xlabel('write count')
plt.ylabel('cdf')
plt.legend()
plt.savefig('ycsb_a_write_cdf.png')
plt.close()
plt.figure()

# first 95% of rows
df_count_sort_95 = df_count_sort.iloc[:int(count_row * 0.95)]
cdf_x = np.array(df_count_sort_95['counts'])
cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df), len(df_count)))
plt.xlabel('write count')
plt.ylabel('cdf')
plt.legend()
plt.savefig('ycsb_a_write_cdf_95.png')
plt.close()

# selected_rows = df_count_sort.loc[df_count_sort['counts'] >=2 ]
# selected_rows.shape
GetWriteCDFForEachKey()




    

