import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import bisect
import os
import bisect
import sys

op_valid_duration_file = "/mnt/nvme0n1/mlsm/test_blob_no_model/with_gc_1.0_0.8/op_valid_duration.txt"



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


def GetValidDurationForKeyRange():
    df = pd.read_csv(op_valid_duration_file, sep=" ")


    valid_duration = df['valid_duration'].to_list()
    valid_duration_x = np.sort(valid_duration)
    valid_duration_y = 1. * np.arange(len(valid_duration_x)) / (len(valid_duration_x)-1)
    plt.figure()
    plt.plot(valid_duration_x, valid_duration_y, label="valid_duration")
    plt.legend()
    plt.savefig("op_valid_duration.png")



    key_range_op_valid_duration = {}
    key_range_group_df = df.groupby(['key_range_idx'])
    plt.figure()
    for key_range_idx, key_range_group in key_range_group_df:
        valid_duration = key_range_group['valid_duration'].to_list()
        valid_duration_x = np.sort(valid_duration)
        valid_duration_y = 1. * np.arange(len(valid_duration_x)) / (len(valid_duration_x)-1)
        plt.plot(valid_duration_x, valid_duration_y, label="key_range_idx: {}, count: {}".format(key_range_idx, len(valid_duration) ))
        key_range_op_valid_duration[key_range_idx] = (valid_duration_x, valid_duration_y)


    plt.legend()
    plt.savefig("op_valid_duration_key_range.png")



