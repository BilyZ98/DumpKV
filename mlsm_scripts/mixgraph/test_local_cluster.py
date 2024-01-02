from dask.distributed import Client, LocalCluster

# Set up a cluster with 4 workers. Each worker uses 1 thread and has a 64GB memory limit.
import dask.array as da
# Do some work
# ...
if __name__ == '__main__':
    cluster = LocalCluster(n_workers=3, threads_per_worker=10, memory_limit='64GB')
    client = Client(cluster)


    array = da.ones((1000, 1000, 1000))
    # res = client.submit(da.sum, array)
    res = array.sum().compute()
    print(res)
    # print(res.result())  # Should print 1.0

    print(array.mean().compute())  # Should print 1.0

    # Close workers and cluster
    client.close()
    cluster.close()

