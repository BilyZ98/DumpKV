
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import bisect
import os
import sys
import multiprocessing as mp
from multiprocesspandas import applyparallel

global_write_count = 0
global_read_count = 0
global_everywrite_write_size = []
global_everywrite_lifetime = []
global_key_write_infos = {}
global_key_read_infos = {}

from multiprocessing import Value, Pool
counter = Value('i', 0)
def increment_counter():
    with counter.get_lock():
        counter.value += 1
    return counter.value

def init_counter(args):
    global counter
    counter = args




def get_lifetime(group_df):
    # group_df.insert(0, "prev_Timestamp", group_df['Timestamp'].shift(1))
    prev_Timestamp = group_df['Timestamp'].shift(1)
    # group_df.insert(0, "lifetime", group_df['Timestamp'] - group_df['prev_Timestamp'])
    lifetime = group_df['Timestamp'] - prev_Timestamp
    # return group_df['lifetime']
    cur_counter = increment_counter()
    if cur_counter % 1000 == 0 :
        print('cur_counter: ', cur_counter)


    return lifetime




def GetLifetimeForWrite(data_paths):
    global global_write_count
    global global_read_count
    global global_everywrite_write_size
    global global_everywrite_lifetime
    global global_key_write_infos
    global global_lifetime
    
    print("read", data_paths[0])
    # print("read", data_paths[1])
    
    dfs = [
        pd.read_csv(data_paths[0], sep=r'\s+', names=['OperationType', 'key']),
        # pd.read_csv(data_paths[1], sep=r'\s+', names=['OperationType', 'key']),
   ]
    df = pd.concat(dfs, ignore_index=True)
    
    df = df.dropna() # delete nan
    print("df.size():", len(df))
    print(df)
    
    # group_by_key
    df_group_by_type = df.groupby('OperationType')
    # df_writes = [df_group_by_type.get_group('INSERT'), df_group_by_type.get_group('UPDATE')]
    df_writes = [ df_group_by_type.get_group('UPDATE')]
    df_read = df_group_by_type.get_group('READ')
    df_write = pd.concat(df_writes, ignore_index=True)
    
    # add sequence number to df_write
    df_write.insert(0, "Timestamp", range(1, len(df_write) + 1))

    global_write_count += len(df_write)
    global_read_count += len(df_read)
    
    print('count of write: ', len(df_write))
    print('count of read: ', len(df_read))
    
    df_write_group_by_key = df_write.groupby('key')
    # get fir 100 df group from write
    df_write_group_by_key_100 = df_write_group_by_key.head(100)
    print(df_write_group_by_key_100)
    # set breakpoint
    # breakpoint()


    df_read_group_by_key = df_read.groupby('key')
    
    print("write key.size():",len(df_write['key'].unique()))


    global_lifetime =df_write_group_by_key_100.apply(get_lifetime )


    # for group_name in df_write['key'].unique():
    #     # group_name: key id, group_df: 
    #     group_df = df_write_group_by_key.get_group(group_name)
    #     group_df.insert(0, "prev_Timestamp", group_df['Timestamp'].shift(1))
    #     group_df.insert(0, "lifetime", group_df['Timestamp'] - group_df['prev_Timestamp'])

    #     global_everywrite_lifetime.extend(group_df['lifetime'].tolist())

    #     write_times = len(group_df)
    #     mean_lifetime = group_df['lifetime'].mean()
    #     
    #     global_key_write_infos[group_name] = [0, write_times, mean_lifetime]
    #     
    print("write analyse done")
    
    # print("read key.size():",len(df_read['key'].unique()))
    # for group_name in df_read['key'].unique():
    #     group_df = df_read_group_by_key.get_group(group_name)
    #     read_times = len(group_df)

    #     global_key_read_infos[group_name] = read_times
    #         
    # print("read analyse done")
      
data_path_list = [
    # "/mnt/nvme1n1/zt/ycsb-workload-gen/data/workloada-load-10000000-10000000.log.formated",
    "/mnt/nvme1n1/zt/YCSB-C/data/workloada-load-10000000-50000000.log_run.formated"
]

GetLifetimeForWrite(data_path_list)

with open('output.txt','w') as f:
    f.write(f'count of write: {global_write_count}\n')
    f.write(f'count of read: {global_read_count}\n')
    # f.write(f'Overwrite ratio: {1 - (len(global_key_write_infos) / global_write_count)}\n')
    f.write(f'Overread ratio: {1 - (len(global_key_read_infos) / global_read_count)}\n')
    
print("save text output")
    
# cdf of write count
# cdf_x = np.sort(np.array(list(global_key_write_infos.values()))[:,1])
# cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
# plt.figure()
# plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(global_write_count, len(global_key_write_infos)))
# plt.xlabel('write count')
# plt.ylabel('cdf')
# plt.legend()
# save_path = 'ycsb_a_0.2_zipfian_seq_write_count.png'
# plt.savefig(save_path)
# plt.close()
# print("save ssd_write_count")

# cdf of write count 95
# cdf_x = cdf_x[:int(len(cdf_x) * 0.95)]
# cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
# plt.figure()
# plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(global_write_count, len(global_key_write_infos)))
# plt.xlabel('write count')
# plt.ylabel('cdf')
# plt.legend()
# save_path = 'ycsb_a_0.2_zipfian_seq_write_count_95.png'
# plt.savefig(save_path)
# plt.close()
# print("save ssd_write_count_95")

# cdf of read count
# cdf_x = np.sort(np.array(list(global_key_read_infos.values())))
# cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
# plt.figure()
# plt.plot(cdf_x, cdf_y, label='overall read:{}, key:{}'.format(global_read_count, len(global_key_read_infos)))
# plt.xlabel('read count')
# plt.ylabel('cdf')
# plt.legend()
# save_path = 'ssd_read_count.png'
# plt.savefig(save_path)
# plt.close()
# print("save ssd_read_count")

# # cdf of read count 95
# cdf_x = cdf_x[:int(len(cdf_x) * 0.95)]
# cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
# plt.figure()
# plt.plot(cdf_x, cdf_y, label='overall read:{}, key:{}'.format(global_read_count, len(global_key_read_infos)))
# plt.xlabel('read count')
# plt.ylabel('cdf')
# plt.legend()
# save_path = 'ssd_read_count_95.png'
# plt.savefig(save_path)
# plt.close()
# print("save ssd_read_count_95")

# lifetime
# arr = np.array(global_everywrite_lifetime)
arr = np.array(global_lifetime)
nan_indices = np.isnan(arr)
print('nan count:', np.sum(nan_indices))
arr_without_nan = arr[~nan_indices]
cdf_x = np.sort(arr_without_nan)
cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
plt.figure()
plt.plot(cdf_x, cdf_y, label='CDF for lifetime')
plt.xlabel('lifetime')
plt.ylabel('cdf')
plt.legend()
save_path = 'ycsb_a_0.2_zipfian_50M_lifetime_cdf.png'
plt.savefig(save_path)
plt.close()
print("save lifetime_cdf")

# lifetime 95
cdf_x = cdf_x[:int(len(cdf_x) * 0.95)]
cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
plt.figure()
plt.plot(cdf_x, cdf_y, label='CDF for lifetime')
plt.xlabel('lifetime')
plt.ylabel('cdf')
plt.legend()
save_path = 'ycsb_a_0.2_zipfian_50M_lifetime_95_cdf.png'
plt.savefig(save_path)
plt.close()
print("save lifetime_cdf_95")

# mean lifetime
plt.figure()
all_mean_lifetimes_time = np.sort(np.array(list(global_key_write_infos.values()))[:,2])
plt.plot(range(len(all_mean_lifetimes_time)), all_mean_lifetimes_time, label='mean lifetime')
plt.xlabel('key')
plt.ylabel('mean lifetime')
plt.legend()
save_path = 'ycsb_a_0.2_zipfian_50M_mean_lifetime.png'
# save_path = 'mean_lifetime.png'
plt.savefig(save_path)
plt.close()
print("save mean lifetime")
