import numpy as np
import matplotlib.pyplot as plt
import dask.dataframe as dd
import bisect
import os
import bisect
import sys
import multiprocessing as mp

folder_path = "/mnt/nvme1n1/workloads/ssd_rocksdb_ycsb_a/"
sub_trace_nums = ["{:02d}".format(i) for i in range(0, 27)]

write_symbols = {'W', 'WFS', 'WM', 'WS'}
read_symbols = {'R', 'RSM', 'RM', 'RS'}
column_names = ['dev_major_minor', 'cpu_core_id', 'record_id', 'timestamp', 'process_id', 'trace_action', 'op_type', 'sector_num', 'plus_symbol', 'io_size', 'proc_name']
prefix = "ssdtrace-"
def AnalyzeWrite():
    pass
dfs = []

files = []
for sub_trace_num in sub_trace_nums:
    files.append(folder_path + prefix + sub_trace_num)
# for sub_trace_num in sub_trace_nums:

#     data_path = folder_path + prefix +  sub_trace_num 
#     df = pd.read_csv(data_path, sep='\\s+', header=None, names=column_names)
#     dfs.append(df)

print('files: ', files)
# df = dd.read_csv(folder_path + prefix +  "*", sep='\\s+', header=None, names=column_names)
df = dd.read_csv(files, sep='\\s+', header=None, names=column_names)
# df_concat = pd.concat(dfs )
# df_group_by_type = df_concat.groupby('op_type')
df_group_by_type = df.groupby('op_type')

# df_write = df_group_by_type.filter(lambda x: x['op_type'] in  write_symbols)
# df_group_by_type_write= [group for name, group in df_group_by_type if name in write_symbols]
df_group_by_type_write= []
for write_symbol in write_symbols:
    try:
        group_df = df_group_by_type.get_group(write_symbol).compute()
        if group_df.empty:
            continue
        df_group_by_type_write.append(group_df)
    except KeyError:
        print('key error:{}'.format(write_symbol))


# [group for name, group in df_group_by_type if name in write_symbols]

df_write = dd.concat(df_group_by_type_write)
print('count of write: ', len(df_write))
df_group_by_type_read = []
for read_symbol in read_symbols:
    try:
        group_df = df_group_by_type.get_group(read_symbol).compute()
        if group_df.empty:
            continue
        df_group_by_type_read.append(group_df)
    except KeyError:
        print('key error:{}'.format(read_symbol))
df_read = dd.concat(df_group_by_type_read)
print('count of read: ', len(df_read))

# df_write_group = pd.concat(df_write)
df_write_group_by_disknumber_offset = df_write.groupby(['dev_major_minor', 'sector_num'])
# df_write_group_by_disknumber_offset_count = df_write_group_by_disknumber_offset.size().reset_index(name='counts')
df_write_group_by_disknumber_offset_count = df_write_group_by_disknumber_offset.size().reset_index().rename(columns={0:'counts'})
df_write_group_by_disknumber_offset_count_sort = df_write_group_by_disknumber_offset_count.sort_values(by=['counts'], ascending=True).compute()
# num_row, num_col = df_write_group_by_disknumber_offset_count.shape.compute()
num_row = len(df_write_group_by_disknumber_offset_count_sort)
cdf_x = np.array(df_write_group_by_disknumber_offset_count_sort['counts'])
cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df_write), len(df_write_group_by_disknumber_offset_count)))
plt.xlabel('write count')
plt.ylabel('cdf')
plt.legend()
save_path = 'ssd_rocksdb_ycsb_a_write_cdf.png'
plt.savefig(save_path)
plt.close()
plt.figure()
row_95 = int(num_row * 0.95)
df_write_group_by_disknumber_offset_count_sort_95 = df_write_group_by_disknumber_offset_count_sort.iloc[:row_95]
cdf_x = np.array(df_write_group_by_disknumber_offset_count_sort_95['counts'])
cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df_write), len(df_write_group_by_disknumber_offset_count_sort_95)))
plt.xlabel('write count')
plt.ylabel('cdf')
plt.legend()
save_path = 'ssd_rocksdb_ycsb_a_write_cdf_95.png'
plt.savefig(save_path)
plt.close()
print('count of disknumber_offset:{}'.format(len(df_write_group_by_disknumber_offset_count)))


AnalyzeWrite()





