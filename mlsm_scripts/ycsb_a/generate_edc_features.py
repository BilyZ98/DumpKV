

import pandas as pd
import pdb
import numpy as np
import matplotlib.pyplot as plt
import os
import sys
import bisect
import multiprocessing as mp
import time
from dask.multiprocessing import get
from multiprocesspandas import applyparallel

from multiprocessing import Value, Pool

max_hash_edc_idx = 65535
base_edc_window = 10
n_edc_feature = 10
delta_count = 32
delta_prefix = 'delta_'
edc_prefix = 'edc_'


data_path = "/mnt/nvme1n1/zt/YCSB-C/data/workloada-load-10000000-50000000.log_run.formated"
output_path = "/mnt/nvme1n1/mlsm/ycsba_0.2_zipfian_50M/features_{}.csv"

hash_edcs = [pow(0.5, i) for i in range(0, max_hash_edc_idx+1)]
edc_windows = [ pow(2, base_edc_window + i) for i in range(0, n_edc_feature)]


delta_columns = [delta_prefix + str(i) for i in range(0, delta_count)]
edc_columns = [edc_prefix + str(i) for i in range(0, n_edc_feature)]

counter = Value('i', 0)
def increment_counter():
    with counter.get_lock():
        counter.value += 1
    return counter.value


def init_edc():
    edc = np.zeros(n_edc_feature)
    return edc

def update_edc(edcs, distance):
    np.seterr(all = 'raise')

    distance_idx = 0
    try:
        for i in range(0, n_edc_feature):
            distance_idx = min(int(distance / edc_windows[i]), max_hash_edc_idx)
            edcs[i] = edcs[i] * hash_edcs[distance_idx] + 1

    except FloatingPointError:
        print('distance: ', distance)
        print('distance_idx: ', distance_idx)
        print('edcs: ', edcs)
        print('edc_windows: ', edc_windows)
        # print('hash_edcs: ', hash_edcs)

       
    return edcs

def GetEDC(df_group_by_disknumber_offset):
    # pdb.set_trace()
    nan_count, non_nan_count = df_group_by_disknumber_offset['lifetime_next'].isnull().sum(), df_group_by_disknumber_offset['lifetime_next'].notnull().sum()
    assert(nan_count == 1)
    # assert(df_group_by_disknumber_offset['lifetime_next'].iloc[-1] == np.nan)
    # assert(pd.isnull(df_group_by_disknumber_offset['lifetime_next'].iloc[-1]))
    
    for i in range(0, delta_count):
        cur_delta_col_name = delta_prefix + str(i)

        df_group_by_disknumber_offset[cur_delta_col_name] = df_group_by_disknumber_offset['lifetime'].shift(i)


    df_group_by_disknumber_offset.fillna(0, inplace=True)

    edc_column_idxs = []
    for edc_column in edc_columns:
        # df_group_by_disknumber_offset[edc_column] = 0
        df_group_by_disknumber_offset.loc[:, edc_column] = float(0.0)
        # df_group_by_disknumber_offset.loc[:, edc_column] = df_group_by_disknumber_offset.loc[:, edc_column].astype(np.float64)
        edc_column_idxs.append(df_group_by_disknumber_offset.columns.get_loc(edc_column))



    # print('key:{}, len:{}'.format(df_group_by_disknumber_offset['offset'].iloc[0], len(df_group_by_disknumber_offset)))

    # pdb.set_trace()
    # df_group_by_disknumber_offset['lifetime'].iloc[0] = np.inf
    edcs = init_edc()
    cur_edcs =  None
    for index in range(0, len(df_group_by_disknumber_offset)):

        if index == 0:
            cur_edcs = edcs
        else:
            try:
                distance = df_group_by_disknumber_offset['lifetime'].iloc[index] 
                cur_edcs = update_edc(edcs, distance)
                edcs = cur_edcs

            # print('index: ', index, 'row: ', df_group_by_disknumber_offset.iloc[index])



            # cur_lifetime = df_group_by_disknumber_offset['lifetime'].iloc[index]
            # df_group_by_disknumber_offset['exp_decayed_counter'].iloc[index] = df_group_by_disknumber_offset['exp_decayed_counter'].iloc[index-1] * pow(2, -cur_lifetime / pow(2,9+index) ) + 1 
    # Attempt to access a row or column
            except IndexError:
                print("Index is out of bounds.", 'index: ', index, 'row: ', 'len: ', len(df_group_by_disknumber_offset))


        df_group_by_disknumber_offset.iloc[index, edc_column_idxs] = cur_edcs



 
def ApplyGetLifetime(df_group_write, output_path, all_output_path):


    
    # pdb.set_trace()
    #print('columns name ', df_group_by_disknumber_offset)

    # df_group_by_disknumber_offset['prev_timestamp'] = df_group_by_disknumber_offset['unix_timestamp'].shift(1)
    # df_group_by_disknumber_offset['latter_timestamp'] = df_group_by_disknumber_offset['unix_timestamp'].shift(-1)
    # df_group_by_disknumber_offset['lifetime'] = df_group_by_disknumber_offset['unix_timestamp'] - df_group_by_disknumber_offset['prev_timestamp']
    # df_group_by_disknumber_offset['lifetime_next'] = df_group_by_disknumber_offset['latter_timestamp'] - df_group_by_disknumber_offset['unix_timestamp']

    # print('df_group_by_type', df_group_by_type.groups.keys())
    # df_group_read = df_group_by_type.get_group('Read')
    # df_group_write = df_group_by_disknumber_offset.get_group('Write')
    # df_group_read['prev_timestamp'] = df_group_read['unix_timestamp'].shift(1)
    # df_group_read['latter_timestamp'] = df_group_read['unix_timestamp'].shift(-1)
    # df_group_read['lifetime'] = df_group_read['unix_timestamp'] - df_group_read['prev_timestamp']
    # df_group_read['lifetime_next'] = df_group_read['latter_timestamp'] - df_group_read['unix_timestamp']


    df_group_write['prev_timestamp'] = df_group_write['timestamp'].shift(1)
    df_group_write['latter_timestamp'] = df_group_write['timestamp'].shift(-1)
    df_group_write['lifetime'] = df_group_write['timestamp'] - df_group_write['prev_timestamp']
    df_group_write['lifetime_next'] = df_group_write['latter_timestamp'] - df_group_write['timestamp']
    # df_group_write['pre_read_count'] = df_group_write['index'] - df_group_write['index'].shift(1) - 1
    # df_group_write['pre_read_count'] = df_group_write['pre_read_count'].fillna(0)


    # df_group_write = GetEDC(df_group_write)
    GetEDC(df_group_write)
    cur_counter = increment_counter()

    # df_group_write = df_group_write.compute()

    if cur_counter % 1000 == 0 :
        print('cur_counter: ', cur_counter)

    # print('key:{}, len:{}'.format(df_group_by_disknumber_offset['offset'].iloc[0], len(df_group_by_disknumber_offset)))
    with open(output_path, 'a') as f:
        df_group_to_write_without_last_row = df_group_write.iloc[:-1]
        df_group_to_write_without_last_row.to_csv(f, index=False, header=f.tell()==0)

    with open(all_output_path, 'a') as f:
        df_group_write.to_csv(f, index=False, header=f.tell()==0)
    return None
    #return df_group_write



def GenerateFeatures(server_trace ):
    # cur_path =  data_path.format(server_trace)
    #cluster = LocalCluster(n_workers=3, threads_per_worker=1, memory_limit='64GB')
    #client = Client(cluster)
    cur_path = server_trace

    # [key, op_type, cf_id, value_size, timestamp, seq, write_rate]
    df = pd.read_csv(cur_path, sep=' ',header=None, names=['op_type', 'key'  ] )
    df = df.head(1000)
    # df = df.repartition(npartitions=1)
    # df = pd.read_csv(cur_path, header=None, names=['timestamp', 'hostname', 'disknumber', 'type', 'offset', 'size', 'response_time'])
    # df = df.head(1000)
    print('df', df)

    group_by_columns = ['op_type']

    # df_group_by_type = df.groupby(['type'])
    # df_write = df_group_by_type.get_group('Write')
    # df_group_by_disknumber_offset = df_group_by_type.groupby(['disknumber', 'offset'])
    df_group_by_op_type = df.groupby(group_by_columns)
    df_gropu_write = df_group_by_op_type.get_group('UPDATE')
    df_group_by_op_type = df_gropu_write.reset_index(drop=True)
    df_group_by_op_type = df_group_by_op_type.reset_index().rename_axis('timestamp', axis=1)
    print('df_group_by_op_type', df_group_by_op_type.head())

    df_group_by_key = df_group_by_op_type.groupby(['key'])


    
    trace_output_path = output_path.format( 'with_at_least_2_write')
    all_trace_output_path = output_path.format( 'all')
    # timestamp,hostname,disknumber,type,offset,size,response_time,prev_timestamp,lifetime,
    # with open(trace_output_path, 'w') as f:
    #     f.write(','.join(['timestamp', 'hostname', 'disknumber', 'type', 'offset', 'size', 'response_time'  ,'unix_timestamp','prev_timestamp', 'lifetime'] + delta_columns + edc_columns) + '\n')


    with open(trace_output_path, 'w') as f:
        f.truncate(0)

    with open(all_trace_output_path, 'w') as f:
        f.truncate(0)
    
    df_return = df_group_by_key.apply( ApplyGetLifetime, output_path=trace_output_path, all_output_path=all_trace_output_path )
    print('type of df_return: ', type(df_return))
    #df_return = df_group_by_disknumber_offset.apply_parallel( ApplyGetLifetime, output_path=trace_output_path )

    # with dask.config.set(scheduler='threads', num_workers=40, memory_limit='64GB'):
    #     df_return = df_group_by_disknumber_offset.apply( ApplyGetLifetime, output_path=trace_output_path, all_output_path=all_trace_output_path).compute()
    #     print('type of df_return: ', type(df_return))

    #     print('head of df_return: ', df_return.head(), 'len of df_return: ', len(df_return) )


if __name__ == '__main__': 
    # all_traces = ['prxy_0', 'prxy_1', 'prn_0']
    # all_traces = ['prn_0']
    GenerateFeatures(data_path)
    # with mp.Pool(3) as p:
 
