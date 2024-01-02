# from dask_kubernetes import HelmCluster
from dask.distributed import Client
from dask_kubernetes.operator import KubeCluster

import dask.array as da
from dask_kubernetes.operator import KubeCluster

if __name__ == '__main__':
    # cluster = KubeCluster.from_yaml('worker-spec.yaml')
    # cluster = KubeCluster.from_yaml('worker-spec.yaml', namespace="myns")
    # cluster = KubeCluster.from_yaml('worker-spec.yaml', namespace="myns", deploy_mode="remote")
    # cluster = KubeCluster.from_yaml('worker-spec.yaml', namespace="myns", deploy_mode="local")
    # cluster = KubeCluster.from_yaml('worker-spec.yaml', namespace="myns", deploy_mode="remote", host="https://
    # cluster = KubeCluster(name="mycluster",
    #                       image='ghcr.io/dask/dask:latest',
    #                       n_workers=3,
    #                       env={"FOO": "bar"},
    #                       resources={"requests": {"memory": "2Gi"}, "limits": {"memory": "16"}})

# cluster = HelmCluster(release_name="myrelease")
# cluster.scale(10)  # specify number of workers explicitly
    cluster = KubeCluster(custom_cluster_spec='cluster-spec.yaml')
    # cluster = KubeCluster.from_yaml('cluster-spec.yaml')                                      

    cluster.adapt(minimum=0, maximum=10)




    client = Client(cluster)

    # Create a large array and calculate the mean
    array = da.ones((1000, 1000, 1000))
    res = array.sum().compute()
    print(res)
    # res = client.submit(sum, array)
    # print(res.result())  # Should print 1.0

    print(array.mean().compute())  # Should print 1.0
    client.close()
    cluster.close()
