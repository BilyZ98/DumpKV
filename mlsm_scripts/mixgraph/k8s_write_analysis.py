import numpy as np
import pandas as pd
import numpy as np


from dask.distributed import Client, LocalCluster
from distributed import PipInstall
import matplotlib.pyplot as plt
from dask_kubernetes import KubeCluster
from dask.distributed import Client
import dask.array as da
import dask.dataframe as dd

import pandas as pd
import bisect
import os
import bisect
import sys
import multiprocessing as mp



data_path = "/mnt/nvme1n1/mlsm/test_blob_no_model_mixgraph/with_gc_1.0_0.8/trace-human_readable_trace.txt"
output_dir= "/mnt/nvme1n1/mlsm/test_blob_no_model_mixgraph/with_gc_1.0_0.8/"
lifetime_cdf_write_path = os.path.join(output_dir, 'lifetime_cdf_write.png')
names = ['key', 'op_type', 'cf_id', 'value_size', 'timestamp', 'seq', 'write_rate']


def test_dask():
    # cluster = KubeCluster(pod_template="worker-spec.yaml" )
    cluster = LocalCluster(n_workers=3, threads_per_worker=10, memory_limit='64GB')

    cluster.adapt(minimum=0, maximum=10)
    client = Client(cluster)
    # pip_installer = PipInstall( )
    # packages=extra_pip_packages, pip_options=extra_pip_install_options
    # client.register_worker_plugin(pip_installer)
    # client.run_on_scheduler(pip_installer.install)


        # Create a pandas DataFrame
    df = pd.DataFrame(np.random.normal(0, 1, (5, 2)), columns=["A", "B"])

    # Convert the pandas DataFrame to a dask DataFrame
    ones = da.ones((10000, 100000, 1000))
    print(ones.mean().compute())  # Should print 1.0
    df_dask = dd.from_pandas(df, npartitions=3)
    sum = df_dask.sum().compute()
    print(sum)

    print(df_dask)
    # res = client.submit(print, df_dask)

    # Get the number of rows
    # num_rows = len(df_dask)
    # or
    # num_rows = len(df_dask.index)
    # print(num_rows)
    client.shutdown()

def GetLifetimeCDFWrite(server_trace, output_dir):
    cluster = LocalCluster(n_workers=3, threads_per_worker=10, memory_limit='64GB')

    cluster.adapt(minimum=0, maximum=10)
    client = Client(cluster)

    # cluster = KubeCluster(pod_template="worker-spec.yaml" )
    # cluster.adapt(minimum=0, maximum=10)
    # client = Client(cluster)

    df = dd.read_csv(server_trace, sep=' ', header=None, names=names)
    write_value = 1
    df_write = df[df['op_type'] == write_value]
    # df_write = df.persist()
    print('df_write ', df_write.loc[2])
    # print('count of write: ', len(df_write))
    df_write['prev_timestamp'] = df_write['timestamp'].shift(1)
    df_write['lifetime'] = df_write['timestamp'] - df_write['prev_timestamp']
    df_write = df_write.persist()
    print('dfwritetype: ', type(df_write))
    print('df_write: ', df_write)
    

    # df_write = df_write.dropna()
    print('count of write: ', len(df_write))

    # avg_size = df_write['value_size'].mean().compute()
    # print('avg of size: ', avg_size)
    # print('avg of lifetime: ', df_write['lifetime'].mean())
    # print('max of lifetime: ', df_write['lifetime'].max())
    # print('min of lifetime: ', df_write['lifetime'].min())
    # print('std of lifetime: ', df_write['lifetime'].std())
    plt.figure()
    plt.xlabel('lifetime')
    plt.ylabel('cdf')
    plt.title('lifetime cdf of write')

    cdf_x = df_write['lifetime'].values
    cdf_x = np.sort(cdf_x)
    cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
    # df_write['lifetime'].hist(cumulative=True, density=1, bins=1000)
    plt.plot(cdf_x, cdf_y)

    plt.savefig(os.path.join(output_dir, 'lifetime_cdf_write.png'))
    plt.close()
    cdf_x_95 = cdf_x[int(len(cdf_x) * 0.95)]
    print('95% of lifetime: ', cdf_x_95)
    cdf_x_95 = cdf_x[:int(len(cdf_x) * 0.95)]
    cdf_y_95 = 1. * np.arange(len(cdf_x_95)) / (len(cdf_x_95) - 1)
    plt.figure()
    plt.xlabel('lifetime')
    plt.ylabel('cdf')
    plt.title('lifetime cdf of write')
    plt.plot(cdf_x_95, cdf_y_95)
    plt.savefig(os.path.join(output_dir, 'lifetime_cdf_write_95.png'))
    plt.close()


    client.shutdown()
    cluster.close()



def GetLifetimeForWrite(server_trace):
    cluster = KubeCluster(pod_template="worker-spec.yaml" )
    cluster.adapt(minimum=0, maximum=10)
    client = Client(cluster)


    # data_path = '/mnt/nvme0n1/workloads/msr/MSR-Cambridge/{}.csv'.format(server_trace)
    data_path = server_trace
    df = dd.read_csv(data_path, sep=' ', header=None, names=names)

    df_group_by_type = df.groupby('op_type')
    write_value = 1
    read_value = 0
    df_write = df_group_by_type.get_group(write_value)
    df_read = df_group_by_type.get_group(read_value)
    print('count of write: ', len(df_write))
    print('count of read: ', len(df_read))
    print('avg of size: ', df_write['size'].mean())
    groupby_key = ['key', 'cf_id']
    df_group_by_disknumber_offset = df_write.groupby(groupby_key)
    group_offset_arr = np.array([])
    group_mean_lifetime_arr = np.array([])
    all_lifetime_arr = np.array([])
    # count = len(df_group_by_disknumber_offset)
    count = 10000
    print('count of group: ', count)
    cur_count = 0
    plt.figure()
    plt.xlabel('lifetime')
    plt.ylabel('cdf')   
    for key, item in df_group_by_disknumber_offset:
        if cur_count >= count:
            break
        cur_count += 1
        if cur_count % 100 == 0:
            print('cur_count: ', cur_count)
        item['prev_timestamp'] = item['timestamp'].shift(1)
        item['lifetime'] = item['timestamp'] - item['prev_timestamp']
        mean_lifetime = item['lifetime'].mean()
        non_nan_count, nan_count = item['lifetime'].count(), item['lifetime'].isna().sum()
        assert(nan_count == 1)
        assert(item['lifetime'].isna().iloc[0] == True)

        # print('key: ', key, 'mean lifetime: ', mean_lifetime, 'count: ', len(item))
        group_offset_arr = np.append(group_offset_arr, key[1])
        group_mean_lifetime_arr = np.append(group_mean_lifetime_arr, mean_lifetime)
        all_lifetime_arr = np.append(all_lifetime_arr, item['lifetime'])
        # if non_nan_count ==  0:
        #     continue

        # 
        # cdf_x = np.sort(np.array(item['lifetime']))
        # # if cdf_x == None:
        # #     print('key:{}, item:\n {}  '.format(key, item))
        # #     print('cdf_x: ', cdf_x)
        # #     continue

        # cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
        # plt.plot(cdf_x, cdf_y, label='write:{}, key:{}'.format(len(item), key))

        # plt.legend()



        # print('mean lifetime: ', mean_lifetime, )
        
    save_path = 'mixgraph_write_all_keys_cdf_{}.png'.format()
    # plt.savefig(save_path)
    plt.close()

    cdf_x = np.sort(all_lifetime_arr)
    cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
    plt.plot(cdf_x, cdf_y, label='write:{}, key:{}'.format(len(all_lifetime_arr), 'all'))
    plt.legend()
    save_path = 'mixgraph_write_all_cdf_{}.png'.format( )
    plt.savefig(save_path)
    plt.close()


    plt.figure()
    plt.plot(group_offset_arr, group_mean_lifetime_arr, ',')
    plt.xlabel('offset')
    plt.ylabel('mean lifetime')
    save_path = 'mixgraph_write_lifetime_{}.png'.format()
    plt.savefig(save_path)
    plt.close()
    client.shutdown()


def GetReadRatio(server_trace):
    data_path = '/mnt/nvme0n1/workloads/msr/MSR-Cambridge/{}.csv'.format(server_trace)
    df = pd.read_csv(data_path, header=None, names=['timestamp', 'hostname', 'disknumber', 'type', 'offset', 'size', 'response_time'])
    df_group_by_type = df.groupby('type')
    df_read = df_group_by_type.get_group('Read')
    print('count of read: ', len(df_read))
    print('avg of size read: ', df_read['size'].mean())
    df_group_by_disknumber_offset = df_read.groupby(['disknumber', 'offset'])
    df_group_by_disknumber_offset_count = df_group_by_disknumber_offset.size().reset_index(name='counts')
    df_group_by_disknumber_offset_count_sort = df_group_by_disknumber_offset_count.sort_values(by=['counts'], ascending=True)
    num_row, num_col = df_group_by_disknumber_offset_count.shape
    cdf_x = np.array(df_group_by_disknumber_offset_count_sort['counts'])
    cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
    plt.plot(cdf_x, cdf_y, label='read:{}, key:{}'.format(len(df_group_by_disknumber_offset_count), 'all'))
    plt.xlabel('write count')
    plt.ylabel('cdf')
    plt.legend()
    save_path = 'msr_read_count_cdf_{}.png'.format(server_trace)
    plt.savefig(save_path)
    plt.close()

def GetOverwriteRatio():
    df = pd.read_csv(data_path, header=None, names=['timestamp', 'hostname', 'disknumber', 'type', 'offset', 'size', 'response_time'])
    df_group_by_type = df.groupby('type')
    df_write = df_group_by_type.get_group('Write')
    df_read = df_group_by_type.get_group('Read')
    print('count of write: ', len(df_write))
    print('count of read: ', len(df_read))
    print('avg of size: ', df_write['size'].mean())
    df_group_by_disknumber_offset = df_write.groupby(['disknumber', 'offset'])
    df_group_by_disknumber_offset_count = df_group_by_disknumber_offset.size().reset_index(name='counts')
    df_group_by_disknumber_offset_count_sort = df_group_by_disknumber_offset_count.sort_values(by=['counts'], ascending=True)
    num_row, num_col = df_group_by_disknumber_offset_count.shape
    cdf_x = np.array(df_group_by_disknumber_offset_count_sort['counts'])
    cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
    plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df_write), len(df_group_by_disknumber_offset_count)))
    plt.xlabel('write count')
    plt.ylabel('cdf')
    plt.legend()
    save_path = 'msr_write_cdf_{}.png'.format(server_trace)
    plt.savefig(save_path)
    plt.close()
    plt.figure()
    df_group_by_disknumber_offset_count_sort_95 = df_group_by_disknumber_offset_count_sort.iloc[:int(num_row * 0.95)]
    cdf_x = np.array(df_group_by_disknumber_offset_count_sort_95['counts'])
    cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
    plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df_write), len(df_group_by_disknumber_offset_count)))
    plt.xlabel('write count')
    plt.ylabel('cdf')
    plt.legend()
    save_path = 'msr_write_cdf_95_{}.png'.format(server_trace)
    plt.savefig(save_path)
    plt.close()

    df_write_size_sort = df_write.sort_values(by=['size'], ascending=True)
    num_row, num_col = df_write_size_sort.shape
    cdf_x = np.array(df_write_size_sort['size'])
    cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
    plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df_write), len(df_group_by_disknumber_offset_count)))
    plt.xlabel('write size')
    plt.ylabel('cdf')
    plt.legend()
    save_path = 'msr_write_size_cdf_{}.png'.format(server_trace)
    plt.savefig(save_path)
    plt.close()

    plt.figure()
    df_write_95 = df_write_size_sort.iloc[:int(num_row * 0.95)]
    num_row, num_col = df_write_95.shape
    cdf_x = np.array(df_write_95['size'])
    cdf_y = 1. * np.arange(len(cdf_x)) / (len(cdf_x) - 1)
    plt.plot(cdf_x, cdf_y, label='overall write:{}, key:{}'.format(len(df_write), len(df_group_by_disknumber_offset_count)))
    plt.xlabel('write size')
    plt.ylabel('cdf')
    plt.legend()
    save_path = 'msr_write_size_cdf_95_{}.png'.format(server_trace)
    plt.savefig(save_path)
    plt.close()



     


    print('count of disknumber_offset: ', len(df_group_by_disknumber_offset))


# ProcessAllTraces()
if __name__ == "__main__":
    # test_dask()
    GetLifetimeCDFWrite(data_path, output_dir)


