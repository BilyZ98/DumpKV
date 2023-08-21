
input_file="/tmp/mlsm/with_gc/trace/trace-put-0-time_series.txt"
        


# Read the file
with open(input_file, 'r') as f:
    lines = f.readlines()
    keys_access_info = {}
    for line in lines:
        type_id, access_time, key = line.split()
        if key not in keys_access_info:
            keys_access_info[key] = {'start_time': int(access_time), 'end_time': int(access_time), 'access_times': 0}
        keys_access_info[key]['end_time'] = int(access_time)
        keys_access_info[key]['access_times'] += 1




# Calculate the lifetime of each key
key_lifetime = {}
key_avg_lifetime = {}
for key in keys_access_info:
    if keys_access_info[key]['access_times'] > 1:
        key_avg_lifetime[key] = (keys_access_info[key]['end_time'] - keys_access_info[key]['start_time']) / keys_access_info[key]['access_times']
    # else:
        # key_avg_lifetime[key] = keys_access_info[key]['end_time'] - keys_access_info[key]['start_time']

    key_lifetime[key] = keys_access_info[key]['end_time'] - keys_access_info[key]['start_time']



out_file = "./key_lifetime.txt"
with open(out_file, 'w') as f:
    for key in key_avg_lifetime:
        f.write(key + " " + str(key_avg_lifetime[key]) + "\n")

print("The number of keys: ", len(key_lifetime))
exit()
# plot the avg lifetime of each key 
import matplotlib.pyplot as plt
import numpy as np
# lines graph of the data and output to a file
# plt.plot(key_avg_lifetime.values(), 'ro')
# keys = (list(key_avg_lifetime.keys()))
# values = (list(key_avg_lifetime.values()))

plt.plot(key_avg_lifetime.keys(), key_avg_lifetime.values(), 'ro')
plt.ylabel('Avg Lifetime')
# plt.show()
plt.savefig('avg_lifetime.png')

# the histogram of the data
# n, bins, patches = plt.hist(key_avg_lifetime.values(), 50, normed=1, facecolor='green', alpha=0.75)



