import numpy as np
import multiprocessing as mp
import matplotlib.pyplot as plt
import pandas as pd
import bisect
import os
import bisect
import sys

M = 1024 * 1024
G = 1024 * M


def mul():

    import pandas as pd
    import numpy as np
    from multiprocessing import Pool, cpu_count

    # Suppose you have a DataFrame 'df'
    df = pd.DataFrame({
        'A': ['foo', 'bar', 'foo', 'bar', 'foo', 'bar', 'foo', 'foo'],
        'B': [1, 2, 3, 4, 5, 6, 7, 8]
    })

    # Group the DataFrame by 'A'
    grouped = df.groupby('A')

    print(grouped)
    # Define a function to apply to each group
    def process_group(group):
        # Replace this with your actual function
        return group.sum()

    def apply_parallel(grouped_df, func):
        with Pool(cpu_count()) as pool:
            ret_list = pool.map(func, [group for name, group in grouped_df])
        return pd.concat(ret_list)

    # Apply the function to each group in parallel
    result = apply_parallel(grouped, process_group)

    print(result)

    exit(0)


data_path = "/mnt/nvme0n1/workloads/cluster/cluster12.sort"
def GetPrevTime(group):
    group.loc[:,'prev_write_time'] = group['time'].shift()
    group['prev_write_time'] = group['prev_write_time'].fillna(group['time'])
    group = group.iloc[1:]
    # group['valid_duration'] = group['time'] - group['prev_write_time']
    group.loc[:, 'valid_duration'] = group.loc[:, 'time'] - group.loc[:, 'prev_write_time']
    # group['time'] - group['prev_write_time']
    return group

def SortByTime(group):
    group = group.sort_values(by=['time'])
    return group

def _apply_df(args):
    df, func  = args
    return func(df)

def GetValidDurationForKeyRange():
    chunk_size = 10000000
    key_range_count = 100
    selected_key_range_group = {1, 10, 20, 30}
    for chunk in pd.read_csv(data_path, chunksize=chunk_size, delimiter=','):
        chunk.columns = ['time', 'key_id', 'key_size', 'value_size', 'client_id', 'op_type', 'ttl']
        grouped_chunks = chunk.groupby('op_type')
        set_group = grouped_chunks.get_group('set')
        set_group_sort = set_group.sort_values(by=['key_id', 'time'])
        # set_group_sort.reset_index( inplace=True)
        set_group_sort['key_range_idx'] = set_group_sort.index // (len(set_group_sort) // key_range_count)
        print('set_group_sort count: ', len(set_group_sort))
        key_id_group = set_group_sort.groupby('key_id')
        # key_id_group = key_id_group.apply(SortByTime)

        print('group by key_id done')
        print('key_id_group count: ', len(key_id_group))
        exit(0)
        return_group = None
        with mp.Pool(8) as pool:
            ret_list = pool.map(_apply_df, [(group, GetPrevTime) for _, group in key_id_group])
            return_group = pd.concat(ret_list)
        # pool = mp.Pool(8)
        # split_key_id_group = np.array_split(key_id_group, 8, axis=0)
        # result = pool.map(_apply_df, [(group, GetPrevTime) for group in split_key_id_group])
        # print('concat done')
        print('concat key_id_group count: ', len(return_group))

        exit(0)
        # key_id_group =  key_id_group.apply(GetPrevTime)
        #ungroup key_id_group
        key_id_group = key_id_group.reset_index(drop=True)
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
        # first_name_space_group.apply(GetPrevTime)   

        # first_name_space_group['prev_write_time'] = first_name_space_group['time'].shift()
        # first_name_space_group['time'].shift(1)
        # first_name_space_group['prev_write_time'] = first_name_space_group['prev_write_time'].fillna(0)
        # first_name_space_group['valid_duration'] = first_name_space_group['time'] - first_name_space_group['prev_write_time']
        # first_name_space_group['valid_duration'] = first_name_space_group['valid_duration'].apply(lambda x: x / 1000) 


        exit(0)


        # for index, row in set_group.iterrows():
        #     key_split = row['key_id']


GetValidDurationForKeyRange()

        
    

