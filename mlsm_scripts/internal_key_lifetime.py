op_trace_file="/mnt/nvme0n1/mlsm/test_blob/with_gc/trace-human_readable_trace.txt"
compaction_trace_file="/mnt/nvme0n1/mlsm/test_blob/with_gc/compaction_human_readable_trace.txt"




op_keys_access_info = {}
op_keys_with_sequence_info = {}
compaction_keys_access_info = {}
with open(op_trace_file, 'r') as f, open(compaction_trace_file, 'r') as f_compaction:
    lines = f.readlines()
    compactioN_lines = f_compaction.readlines()
    for line in compactioN_lines:
        compaction_trace_infos = line.split()
        key = compaction_trace_infos[0]
        sequence_number = compaction_trace_infos[1]
        compaction_time = compaction_trace_infos[2]
        internal_key = key + "_" + sequence_number
        compaction_keys_access_info[internal_key] = {'compaction_time': int(compaction_time), 'insert_time': None, 'invalid_time': None, 'valid_duration': None, 'actual_lifetime': None, 'gc_time': None }





    for line in lines:
        trace_infos = line.split()
        type_id = trace_infos[1]
        if type_id == '1' or type_id == '2':
            key = trace_infos[0]
            access_time = trace_infos[4]
            sequence_number = trace_infos[5]
            key_with_seq = key + "_" + sequence_number
            if key_with_seq in compaction_keys_access_info:
                compaction_keys_access_info[key_with_seq]['insert_time'] = int(access_time)

            if key not in op_keys_access_info:
                op_keys_access_info[key] = []
            op_keys_access_info[key].append([int(sequence_number), int(access_time)])
            # if key_with_seq not in op_keys_with_sequence_info:
            #     op_keys_with_sequence_info[key_with_seq] = {'insert_time': int(access_time), 'compaction_time': None, }
            #     keys_access_info[key_with_seq] = {'insert_time': int(access_time), 'compaction_time': None, }

    for key, access_infos in op_keys_with_sequence_info:
        sorted(access_infos, key=lambda x: x[0])
        for i in range(len(access_infos) - 1):
            internal_key = key + "_" + str(access_infos[i][0])
            valid_duration = access_infos[i + 1][1] - access_infos[i][1]
            invalid_time = access_infos[i + 1][1]
            if internal_key in compaction_keys_access_info:
                assert(compaction_keys_access_info[internal_key]['insert_time'] == access_infos[i][1])
                assert(compaction_keys_access_info[internal_key]['invalid_time'] == None)
                compaction_keys_access_info[internal_key]['invalid_time'] = invalid_time
                compaction_keys_access_info[internal_key]['valid_duration'] = valid_duration
                actual_lifetime = compaction_keys_access_info[internal_key]['compaction_time'] - compaction_keys_access_info[internal_key]['insert_time']
                compaction_keys_access_info[internal_key]['actual_lifetime'] = actual_lifetime
            



























