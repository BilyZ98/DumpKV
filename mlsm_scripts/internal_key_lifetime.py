import numpy as np
import bisect
import os
import bisect
import sys
op_trace_file="/mnt/nvme0n1/mlsm/test_blob/with_gc_1.0_0.2/trace-human_readable_trace.txt"
# compaction_trace_file="/mnt/nvme0n1/mlsm/test_blob/with_gc/compaction_human_readable_trace.txt"
compaction_trace_file = "/mnt/nvme0n1/mlsm/test_blob/with_gc_1.0_0.2/compaction_human_readable_trace.txt"

op_trace_dir_prefix="/mnt/nvme0n1/mlsm/test_blob/with_gc_1.0_"
compaction_trace_dir_prefix= op_trace_dir_prefix


key_range_file = "/tmp/mlsm/key_range.txt"
op_valid_duration_file = "op_valid_duration.txt"

key_ranges = []
num_key_ranges = 100
# [0-1) , [1-2) , [2-4), ... [1024, inf)
# lifetime_labels = { 1: 1, 2: 2, 4:3, 8:4, 16:5, 32:6, 64:7, 128:8, 256:9, 512:10, 1024:11 }
# lifetime_limits = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]
lifetime_limits = [1, 10, 100, 1000]

long_lifetime_border = 2 * 1e8

def callGetInternalKeyLifetime(op_trace_file, compaction_trace_file, output_dir):

    cur_push_dir = os.getcwd()
    os.chdir(output_dir)
    GetInternalKeyLifetime(op_trace_file, compaction_trace_file )
    os.chdir(cur_push_dir)


def GetInternalKeyLifetime(op_trace_file, compaction_trace_file ):

    op_keys_access_info = {}
    op_keys_with_sequence_set = set()
    compaction_keys_access_info = {}
    int_key_id = 0
    uniq_key_map = {}
    op_keys_without_seq_count = {}
    start_time = None

    with open(op_trace_file, 'r') as f, open(compaction_trace_file, 'r') as f_compaction:
        lines = f.readlines()
        compaction_lines = f_compaction.readlines()
        for line in compaction_lines:
            compaction_trace_infos = line.split()
            key = compaction_trace_infos[0]
            sequence_number = compaction_trace_infos[1]
            compaction_time = compaction_trace_infos[2]
            int_seq = int(sequence_number)
            assert(int_seq > 0)
            internal_key = key + "_" + sequence_number

            compaction_keys_access_info[internal_key] = {'compaction_time': int(compaction_time), 'insert_time': None, 
                                                         'invalid_time': None, 'valid_duration': None, 
                                                         'actual_lifetime': None , 'uniq_key_id': None,
                                                         'period_num_writes': None,
                                                         'key_range_idx': None}


        print("The number of keys in compaction trace: ", len(compaction_keys_access_info))


        first_line = lines[0]
        trace_infos = first_line.split()
        start_time = int(trace_infos[4])


        for line in lines:
            trace_infos = line.split()
            type_id = trace_infos[1]
            if type_id == '1' or type_id == '2':
                key = trace_infos[0]
                access_time = trace_infos[4]
                sequence_number = trace_infos[5]
                period_num_writes = trace_infos[6]
                int_seq_num = int(sequence_number)
                assert(int_seq_num > 0)
                key_with_seq = key + "_" + sequence_number
                op_keys_without_seq_count[key] = op_keys_without_seq_count.get(key, 0) + 1
                if key_with_seq in compaction_keys_access_info:
                    compaction_keys_access_info[key_with_seq]['insert_time'] = int(access_time)
                    compaction_keys_access_info[key_with_seq]['period_num_writes'] = period_num_writes

                if key not in op_keys_access_info:
                    op_keys_access_info[key] = []
                    int_key_id += 1
                    uniq_key_map[key] = int_key_id
                op_keys_access_info[key].append([int_seq_num, int(access_time), int(period_num_writes)])
                op_keys_with_sequence_set.add(key_with_seq)
                # if key_with_seq not in op_keys_with_sequence_info:
                #     op_keys_with_sequence_info[key_with_seq] = {'insert_time': int(access_time), 'compaction_time': None, }
                #     keys_access_info[key_with_seq] = {'insert_time': int(access_time), 'compaction_time': None, }
        sorted_user_keys = sorted(list(op_keys_access_info.keys()))
        for i in range(num_key_ranges):
            next_key_idx = int(float(i/num_key_ranges) * len(sorted_user_keys))
            key_ranges.append( sorted_user_keys[next_key_idx] )

        arr = [0] * (num_key_ranges + 1)
        for key, count in op_keys_without_seq_count.items():
            key_range_id = bisect.bisect_left(key_ranges, key)
            arr[key_range_id] += count

        for i in range(num_key_ranges):
            print("key_range: {}, count: {}".format(key_ranges[i], arr[i]))

        # exit(0)


        # with open(key_range_file, 'w') as f:
        #     for key in key_ranges:
        #         f.write(key + "\n")
        # exit(0)

        for key, _ in compaction_keys_access_info.items():
            if key not in op_keys_with_sequence_set:
                print("key: {} not int op trace".format(key) )
                # print("comp_key_info: ", comp_key_info)
                assert(False)

        # Todo: get valid duration for keys that are not in compaction but is overwritten.
        print("The number of keys in op trace: ", len(op_keys_access_info))
        for key, access_infos in op_keys_access_info.items():
            # print('key: {}, access_infos: {}'.format(key, access_infos))
            # if len(access_infos) >= 3:
            #     print('key: {}, access_infos: {}'.format(key, access_infos))
            sorted(access_infos, key=lambda x: x[0])
            # if len(access_infos) >= 3:
            #     print('key: {}, access_infos: {}'.format(key, access_infos))
            #     exit()
            for i in range(len(access_infos) - 1):
                internal_key = key + "_" + str(access_infos[i][0])
                valid_duration = access_infos[i + 1][1] - access_infos[i][1]
                seq_i = access_infos[i][0]
                seq_i_1 = access_infos[i + 1][0]
                assert(valid_duration > 0 and seq_i < seq_i_1)
                invalid_time = access_infos[i + 1][1]
                key_range_idx = bisect.bisect_left(key_ranges, key)
                if internal_key in compaction_keys_access_info:
                    assert(compaction_keys_access_info[internal_key]['insert_time'] == access_infos[i][1])
                    assert(compaction_keys_access_info[internal_key]['invalid_time'] == None)
                    compaction_keys_access_info[internal_key]['invalid_time'] = invalid_time
                    compaction_keys_access_info[internal_key]['valid_duration'] = valid_duration
                    actual_lifetime = compaction_keys_access_info[internal_key]['compaction_time'] - compaction_keys_access_info[internal_key]['insert_time']
                    compaction_keys_access_info[internal_key]['actual_lifetime'] = actual_lifetime
                    assert(uniq_key_map.get(key) != None)
                    compaction_keys_access_info[internal_key]['uniq_key_id'] = uniq_key_map[key]
                    compaction_keys_access_info[internal_key]['key_range_idx'] = key_range_idx

                # else:
                    # print("something wrong!")
                    # assert(False)
                
    valid_durations = []
    with open(op_valid_duration_file, 'w') as f:
        f.write('key key_id sequence_number insert_time invalid_time valid_duration period_num_writes is_long_live key_range_idx lifetime_exp_increase_label time_elapse_from_begin\n')
        for key, infos in op_keys_access_info.items():
            for i in range(len(infos) - 1):
                seq_i = infos[i][0]
                seq_i_1 = infos[i + 1][0]
                assert(seq_i < seq_i_1)
                internal_key = key + "_" + str(seq_i)
                invalid_time = infos[i + 1][1]
                key_range_idx = bisect.bisect_left(key_ranges, key)
                is_long_live = 0
                insert_time = infos[i][1]
                valid_duration = invalid_time - insert_time
                cur_key_period_num_writes = infos[i][2]
                key_id = uniq_key_map[key]
                lifetime_label_idx = bisect.bisect_left(lifetime_limits, valid_duration/1e6)

                elapse_from_start_time = insert_time - start_time
                if valid_duration > long_lifetime_border:
                    is_long_live = 1
                f.write('{} {} {} {} {} {} {} {} {} {} {}\n'.format(key, key_id, seq_i, insert_time, invalid_time, valid_duration, cur_key_period_num_writes, is_long_live, key_range_idx, lifetime_label_idx, elapse_from_start_time))
                valid_durations.append(valid_duration)

    exit(0)



    valid_durations = []
    actual_lifetimes = []
    lifetime_gap = []
    lifetime_ratios = []


    output_file_name = "internal_key_lifetime.txt"
    with open(output_file_name, 'w') as f:
        f.write('key sequence_number insert_time compaction_time invalid_time valid_duration actual_lifetime period_num_writes is_long_live key_range_idx lifetime_exp_increase_label\n')
        for key, infos in compaction_keys_access_info.items():

            if infos['uniq_key_id'] != None:
                is_long_live = 0
                if infos['valid_duration'] != None and infos['valid_duration'] > long_lifetime_border:
                    is_long_live = 1
                

                lifetime_label_idx = bisect.bisect_left(lifetime_limits, infos['valid_duration']/1e6)
                # lifetime_label_idx = 0
                # if lifetime_label_idx == len(lifetime_limits):
                #     dataset_label = 12
                # else:
                # cur_lifetime_upper_bound = lifetime_limits[lifetime_label_idx]
                # dataset_label = lifetime_labels[cur_lifetime_upper_bound]
                dataset_label = lifetime_label_idx


                f.write('{} {} {} {} {} {} {} {} {} {} {}\n'.format(infos['uniq_key_id'], key.split('_')[1], infos['insert_time'], infos['compaction_time'], infos['invalid_time'], infos['valid_duration'], infos['actual_lifetime'], infos['period_num_writes'], is_long_live, infos['key_range_idx'], dataset_label))
            else:
                print('key: ', key)
                assert(False)
                # assert(False)
            # assert(infos['valid_duration'] != None)
            if infos['valid_duration'] != None and infos['actual_lifetime'] != None:
                lifetime_gap.append(infos['actual_lifetime'] - infos['valid_duration'])
                # ratio = (infos['actual_lifetime']) / (infos['valid_duration'])
                ratio = (infos['valid_duration']) / (infos['actual_lifetime'])
                lifetime_ratios.append(ratio )

            if infos['valid_duration'] != None:
                valid_durations.append(infos['valid_duration'])

            if infos['actual_lifetime'] != None:
                actual_lifetimes.append(infos['actual_lifetime'])

    # print('lifetime ratio: ', lifetime_ratios)
    print('len of lifetime ratio: ', len(lifetime_ratios))
    sort_lifetime_ratio = sorted(lifetime_ratios)
    print('last 10 of sort lifetime ratio: ', sort_lifetime_ratio[-200:-100])
    index= bisect.bisect_left(sort_lifetime_ratio, 100)
    print(' number of lifetime ratio > 100:{} , ratio is {} '.format(len(sort_lifetime_ratio) - index, index/len(sort_lifetime_ratio) )) 
    print(' number of lifetime ratio > 10:{} , ratio is {} '.format(len(sort_lifetime_ratio) - bisect.bisect_left(sort_lifetime_ratio, 10), bisect.bisect_left(sort_lifetime_ratio, 10)/len(sort_lifetime_ratio) ))
    valid_duration_file = "valid_duration.txt"
    actual_lifetime_file = "actual_lifetime.txt"
    lifetime_gap_file = "lifetime_gap.txt"

    with open(valid_duration_file, 'w') as f:
        for valid_duration in valid_durations:
            f.write(str(valid_duration) + "\n")

    with open(actual_lifetime_file, 'w') as f:
        for actual_lifetime in actual_lifetimes:
            f.write(str(actual_lifetime) + "\n")

    with open(lifetime_gap_file, 'w') as f:
        for gap in lifetime_gap:
            f.write(str(gap) + "\n")

    print("len of valid_duration: {}, len of actual_lifetime: {}, len of lifetime_gap: {}".format(len(valid_durations), len(actual_lifetimes), len(lifetime_gap)))
    import matplotlib.pyplot as plt
    import numpy as np



    valid_durations_x = np.sort(valid_durations)
    valid_durations_y = 1. * np.arange(len(valid_durations)) / (len(valid_durations) - 1)

    actual_lifetimes_x = np.sort(actual_lifetimes)
    actual_lifetimes_y = 1. * np.arange(len(actual_lifetimes)) / (len(actual_lifetimes) - 1)

    lifetime_gap_x = np.sort(lifetime_gap)
    lifetime_gap_y = 1. * np.arange(len(lifetime_gap)) / (len(lifetime_gap) - 1)

    plt.plot(valid_durations_x, valid_durations_y, label='valid_durations')
    plt.plot(actual_lifetimes_x, actual_lifetimes_y, label='actual_lifetimes')
    plt.plot(lifetime_gap_x, lifetime_gap_y, label='lifetime_gap')
    plt.legend()
    plt.savefig('compaction_lifetime.png')

    plt.figure()
    plt.hist([valid_durations, actual_lifetimes, lifetime_gap], bins=20, label=['valid_durations', 'actual_lifetimes', 'lifetime_gap'], histtype='bar')
    # plt.hist(valid_durations, bins=100, label='valid_durations')
    # plt.hist(actual_lifetimes, bins=100, label='actual_lifetimes')
    # plt.hist(lifetime_gap, bins=100, label='lifetime_gap')
    plt.legend()
    plt.savefig('compaction_lifetime_hist.png')

    plt.figure()
    lifetime_ratios_x = np.sort(lifetime_ratios)
    lifetime_ratios_y = 1. * np.arange(len(lifetime_ratios)) / (len(lifetime_ratios) - 1)
    # plt.hist(lifetime_ratios, bins=20,  label='lifetime_ratios' , histtype='bar', density=True) 
    plt.plot(lifetime_ratios_x, lifetime_ratios_y, label='lifetime_ratios')
    plt.legend()
    plt.savefig('compaction_lifetime_ratios_valid_to_actual.png')



    # output_file = "/mnt/nvme0n1/mlsm/test_blob/with_gc/internal_key_lifetime.txt"
    # with open(output_file, 'w') as f:


op_trace = sys.argv[1]
compaction_trace = sys.argv[2]
output_dir = sys.argv[3]
if __name__  == "__main__":
    callGetInternalKeyLifetime(op_trace, compaction_trace, output_dir)


# for i in np.arange(0.0, 1.2, 0.2):
#     gc_threshold = round(i,2)
#     cur_dir = op_trace_dir_prefix + str(gc_threshold)
#     cur_op_trace_file = cur_dir + "/trace-human_readable_trace.txt"
#     cur_compaction_trace_file = cur_dir + "/compaction_human_readable_trace.txt"
#     cur_output_data_dir = './with_gc_1.0_' + str(gc_threshold) + '/'
#     print('cur_op_trace_file: ', cur_op_trace_file)
#     print('cur_compaction_trace_file: ', cur_compaction_trace_file)
#     print('cur_output_data_dir: ', cur_output_data_dir)
#     cur_push_dir = os.getcwd()
#     os.mkdir(cur_output_data_dir ) 
#     os.chdir(cur_output_data_dir)

#     GetInternalKeyLifetime(cur_op_trace_file, cur_compaction_trace_file )
#     os.chdir(cur_push_dir)

    # op_trace_files.append(op_trace_dir_prefix + str(gc_threshold) + "/trace-human_readable_trace.txt")
    # compaction_trace_files.append(compaction_trace_dir_prefix + str(gc_threshold) + "/compaction_human_readable_trace.txt")


#include <LightGBM/prediction_early_stop.h>
#include <LightGBM/dataset.h>
#include <LightGBM/boosting.h>
#include <LightGBM/objective_function.h>
#include <LightGBM/metric.h>

#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>


# int main(int argc, char* argv[]) {
#     // Load model
#     std::unique_ptr<LightGBM::Boosting> boosting(LightGBM::Boosting::CreateBoosting("gbdt", "saved_model.txt"));

#     // Load data
#     std::unique_ptr<LightGBM::Dataset> data(LightGBM::Dataset::CreateFromCSV("test_data.csv"));

#     // Get number of features
#     int num_feature = boosting->NumberOfFeature();

#     // Check number of features in data
#     if (num_feature != data->num_total_features()) {
#         throw std::runtime_error("Number of features in model and data do not match.");
#     }

#     // Get number of data points
#     int num_data = data->num_data();

#     // Create output vector
#     std::vector<double> output(num_data);

#     // Predict
#     boosting->Predict(nullptr, num_iteration, &output);

#     // Load actual values
#     std::vector<double> actual_values = load_actual_values("actual_values.csv");

#     // Calculate accuracy
#     double correct_predictions = 0;
#     for (int i = 0; i < num_data; ++i) {
#         if (output[i] == actual_values[i]) {
#             correct_predictions++;
#         }
#     }
#     double accuracy = correct_predictions / num_data;

#     // Print accuracy
#     std::cout << "Accuracy: " << accuracy << std::endl;

#     return 0;
# }






















