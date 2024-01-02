from dask_kubernetes import KubeCluster
from dask.distributed import Client
import dask.array as da

# Create a Kubernetes cluster
cluster = KubeCluster(pod_template="worker-spec.yaml" )
# namespace="myns"

# Adaptively scale the cluster
cluster.adapt(minimum=0, maximum=10)

# Create a Dask client
client = Client(cluster)

# Create a large array and compute its mean
# array = da.ones((10000, 100000, 1000))
# print(array.mean().compute())  # Should print 1.0
def sqaure(x):
    return x**2

def neg(x):
    return -x

A = client.map(sqaure, range(10))
B = client.map(neg, A)
total = client.submit(sum, B)
print(total.result())  # Should print -285

# Shutdown the client
client.shutdown()

