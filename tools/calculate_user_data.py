from matplotlib import pyplot as plt
import numpy as np
import os
import sys

human_readable_trace = sys.argv[1] 
dir_size_file=sys.argv[2]
space_amp_output_file = sys.argv[3]
global_dict ={}
# my_dict={}
user_space=[]

count_duration = 1.0e7

K = 1024
M = 1024 * K 
with open(human_readable_trace, 'r') as f:
    first_line=f.readline()
    start_ts=int(first_line.split()[4])
    end_ts=start_ts+ count_duration    
    cur_time = start_ts

    cur_valid_size = 0

    lines = f.readlines()
    for line in lines:
        words=line.split()
        cur_time = int(words[4])
        if cur_time>end_ts: 
            #统计当前10s内用户写数据大小
            print('cur_valid_size', cur_valid_size)
            user_space.append(cur_valid_size/M)
            # f.write(str(initial_size)+' '+words[4]+'\n')
            #处理下一个10s间隔
            cur_time = end_ts
            end_ts= end_ts + count_duration
            
        if words[1]== '1' :
            cur_key=words[0]
            cur_value_size =int(words[3])
            #更新cur_key的存储空间
            if cur_key in global_dict :
                cur_valid_size-=global_dict[cur_key]
            else:
                cur_valid_size += len(cur_key)
            global_dict[cur_key]=cur_value_size
            cur_valid_size+=cur_value_size


with  open(dir_size_file, 'r') as f:
    lines=f.readlines()
    i=0
    x = []
    y = []
    len_dir_size_file = len(lines)
    len_user_size = len(user_space)
    print("len_dir_size_file: ", len_dir_size_file)
    print("len_user_size: ", len_user_size)
    for line in lines:
        if i >= len_user_size:
            break
        words=line.split()
        storage_space = int(words[0])
        cur_time = (words[1])
        

        x.append(int(cur_time))
        if user_space[i] == 0:
            space_amplipication = 0
        else:
            space_amplipication = float(storage_space/user_space[i])
        y.append(space_amplipication)
        print(cur_time + ": " , (space_amplipication))
        # f.write(cur_time + " " + str(space_amplipication)+'\n')
        i+=1
    plt.plot(x,y)
    plt.savefig(space_amp_output_file )

        
# with open(space_amp_output_file, 'r') as f:

#     plt.figure()
#     lines=f.readlines()
#     x=[]
#     y=[]
#     for line in lines:
#         words=line.split()
#         x.append(int(words[0]))
#         y.append(float(words[1]))
#     plt.plot(x,y)

