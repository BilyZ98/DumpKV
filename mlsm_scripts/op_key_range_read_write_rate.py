import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import bisect
import os
import sys

op_valid_duration_file = "/mnt/nvme0n1/mlsm/test_blob_no_model/with_gc_1.0_0.8/op_valid_duration.txt"

def GetWriteRateForKeyRange():
    df = pd.read_csv(op_valid_duration_file, sep=" ")
    cur_time = df['insert_time'].iloc[0]
    one_sec_micro = 1e6
    next_time = cur_time + one_sec_micro
    key_range_groups = df.groupby(['key_range_idx'])
    cur_key_range_write = 0;
    write_rate_x = []
    write_rate_y = []
    for key_range_idx, key_range_group in key_range_groups:
        if key_range_idx != 25:
            continue

        sort_by_time_df = key_range_group.sort_values(by=['insert_time'])
        for idx, row in sort_by_time_df.iterrows():
            if row['insert_time'] > next_time:
                write_rate_y.append(cur_key_range_write)
                write_rate_x.append(cur_time)
                cur_time = next_time
                next_time = cur_time + one_sec_micro
                cur_key_range_write = 0
            cur_key_range_write += 1

            # if idx == 0:sd
            #     continue
            # else:
            #     sort_by_time_df.loc[idx, 'write_rate'] = 1. * row['write_count'] / (row['insert_time'] - start_time)

        # write_rate = key_range_group['write_rate'].to_list()

        plt.figure()
        plt.plot(write_rate_x, write_rate_y, label="key_range_idx: {}, count: {}".format(key_range_idx, len(write_rate_x) ))
        plt.legend()
        plt.savefig("op_write_rate_key_range_{}.png".format(key_range_idx))
        exit(0)
        # write_rate_y = 1. * np.arange(len(write_rate_x)) / (len(write_rate_x)-1)
        # plt.figure()
        # plt.plot(write_rate_x, write_rate_y, label="key_range_idx: {}, count: {}".format(key_range_idx, len(write_rate) ))
        # plt.legend()
        # plt.savefig("op_write_rate_key_range_{}.png".format(key_range_idx))
GetWriteRateForKeyRange()
