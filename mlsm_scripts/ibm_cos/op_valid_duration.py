import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import bisect
import os
import bisect
import sys
import multiprocessing as mp



# import pandas as pd
# import matplotlib.pyplot as plt

# # Create a simple pandas DataFrame
# data = {
#     'Year': [2015, 2016, 2017, 2018, 2019],
#     'Sales': [200, 300, 250, 350, 300]
# }
# df = pd.DataFrame(data)

# # Plot the DataFrame
# plt.figure(figsize=(10,6))
# plt.plot(df['Year'], df['Sales'], marker='o')
# plt.ti


M = 1024 * 1024
G = 1024 * M

data_path = '/mnt/nvme0n1/workloads/ibm_cos_trace/IBMObjectStoreTrace007Part0'
def getvaliddurationforkeyrange():
    chunk_size = 1000000
    key_range_count = 100
    selected_key_range_group = {1, 10, 20, 30}
    for chunk in pd.read_csv(data_path, chunksize=chunk_size, delimiter=' '):
        chunk.columns = ['time', 'key_id', 'value_size', 'op_type']
        grouped_chunks = chunk.groupby('op_type')
        set_group = grouped_chunks.get_group(1)
        set_group_sort = set_group.sort_values(by=['key_id', 'time'])
        # set_group_sort.reset_index( inplace=true)
        all_keys = set_group_sort['key_id'].unique()
        num_all = len(all_keys)
        # set_group_sort['key_range_idx'] = set_group_sort.index // (len(set_group_sort) // key_range_count)
        set_group_sort['key_range_idx'] = set_group_sort['key_id'].apply(lambda x: bisect.bisect_left(all_keys, x) // (num_all // key_range_count))
        tail_100 = set_group_sort.iloc[100: 1000]
        print('tail_100 : ', (tail_100))
        key_id_group = set_group_sort.groupby('key_id')
        # key_id_group = key_id_group.apply(sortbytime)

        key_149 = key_id_group.get_group(149) 
        print('key_149  ', (key_149))

        print('group by key_id done')

        print('set_group_sort count: \n', len(set_group_sort))
        print('key_id_group count: ', len(key_id_group))
        exit(0)
        return_group = none
        with mp.pool(8) as pool:
            ret_list = pool.map(_apply_df, [(group, getprevtime) for _, group in key_id_group])
            return_group = pd.concat(ret_list)
        # pool = mp.pool(8)
        # split_key_id_group = np.array_split(key_id_group, 8, axis=0)
        # result = pool.map(_apply_df, [(group, getprevtime) for group in split_key_id_group])
        # print('concat done')
        print('concat key_id_group count: ', len(return_group))

        exit(0)
        # key_id_group =  key_id_group.apply(getprevtime)
        #ungroup key_id_group
        key_id_group = key_id_group.reset_index(drop=true)
        print('key_id_group count: ', len(key_id_group))
        key_id_group_key_range = key_id_group.groupby('key_range_idx')
        for key_range_idx, key_range_group in key_id_group_key_range:
            if key_range_idx not in selected_key_range_group:
                continue
            valid_duration = key_range_group['valid_duration'].to_list()
            valid_duration_x = np.sort(valid_duration)
            valid_duration_y = 1. * np.arange(len(valid_duration_x)) / (len(valid_duration_x)-1)
            plt.figure()
            plt.plot(valid_duration_x, valid_duration_y, label="key_range_idx: {}, count: {}".format(key_range_idx, len(valid_duration) ))
            plt.legend()
            plt.xlabel('valid_duration')
            plt.ylabel('cdf')
            plt.title('key_range_idx: {}, count: {}'.format(key_range_idx, len(valid_duration)))
            plt.savefig('key_range_idx_{}.png'.format(key_range_idx))
            plt.close()

            # key_range_op_valid_duration[key_range_idx] = (valid_duration_x, valid_duration_y)

        # key_id_group['valid_duration'] = key_id_group['valid_duration'].apply(lambda x: x / 1000)
        # df_merged = pd.merge(set_group_sort, key_id_group, on=['time', 'key_id', 'key_size', 'value_size', 'client_id', 'op_type', 'ttl', 'key_range_idx'], how='left')



        # set_group['first_name_space'] = set_group['key_id'].apply(lambda x: x.split('/')[0])
        # first_name_space_group = set_group.groupby('first_name_space')
        # print('first_name_space count: ', len(first_name_space_group))
        # print('set group count: ', len(set_group))
        # first_name_space_group.apply(getprevtime)   

        # first_name_space_group['prev_write_time'] = first_name_space_group['time'].shift()
        # first_name_space_group['time'].shift(1)
        # first_name_space_group['prev_write_time'] = first_name_space_group['prev_write_time'].fillna(0)
        # first_name_space_group['valid_duration'] = first_name_space_group['time'] - first_name_space_group['prev_write_time']
        # first_name_space_group['valid_duration'] = first_name_space_group['valid_duration'].apply(lambda x: x / 1000) 


        exit(0)


        # for index, row in set_group.iterrows():
        #     key_split = row['key_id']


getvaliddurationforkeyrange()

        
    

