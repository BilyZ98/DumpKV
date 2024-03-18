
# read data

# generate past distance data and edcs for each write
max_hash_edc_idx = 65535
base_edc_window = 10
n_edc_feature = 10
delta_count = 10
delta_prefix = 'delta_'
edc_prefix = 'edc_'


hash_edcs = [pow(0.5, i) for i in range(0, max_hash_edc_idx+1)]
edc_windows = [ pow(2, base_edc_window + i) for i in range(0, n_edc_feature)]


delta_columns = [delta_prefix + str(i) for i in range(0, delta_count)]
edc_columns = [edc_prefix + str(i) for i in range(0, n_edc_feature)]



# train partial data and then do prediction for latter part of data.
