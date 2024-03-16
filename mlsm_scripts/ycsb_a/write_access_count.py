

import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import bisect
import os
import sys
import multiprocessing as mp


# data_path = "/mnt/nvme1n1/zt/ycsb-workload-gen/data/workloada-run-10000000-10000000.log.formated"
data_path = "/mnt/nvme1n1/zt/YCSB-C/data/workloada-load-10000000-10000000.log_run.formated"
# output_fig_name = "ycsb_a_write_cdf_10M_10M.png"
output_fig_name = "ycsb_a_write_cdf_zipfian_0.5_10M_10M.png"
# output_fig_95_name = "ycsb_a_write_cdf_zipfian_0.5__10M_10M_95.png"
output_fig_95_name = "ycsb_a_write_cdf_zipfian_0.5_10M_10M_95.png"

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
# key_count = 
plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df), len(df_count)))
plt.xlabel('write count')
plt.ylabel('cdf')
plt.legend()
plt.savefig(output_fig_name)
plt.close()
plt.figure()
sum_of_all_write = df_count_sort['counts'].sum()
# first 95% of rows
df_count_sort_95 = df_count_sort.iloc[:int(count_row * 0.95)]
sum_of_first_95_write = df_count_sort_95['counts'].sum()
print('95% write count:', sum_of_first_95_write)
delta = sum_of_all_write - sum_of_first_95_write
print('top 5% write count:', delta)
cdf_x = np.array(df_count_sort_95['counts'])
cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df), len(df_count)))
plt.xlabel('write count')
plt.ylabel('cdf')
plt.legend()
plt.savefig(output_fig_95_name)
plt.close()

# selected_rows = df_count_sort.loc[df_count_sort['counts'] >=2 ]
# selected_rows.shape
GetWriteCDFForEachKey()





