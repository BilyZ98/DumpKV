from matplotlib import pyplot as plt
import numpy as np
import os
import sys

human_readable_trace = sys.argv[1] 
output_storage=sys.argv[2]
global_dict ={}
my_dict={}
user_space=[]

with open(human_readable_trace) as f:
    first_line=f.readline()
    start_ts=int(first_line.split()[4])
    end_ts=start_ts+1.0e7 

    initial_size=0.0

    lines = f.readlines()
    for line in lines:
        words=line.split()
        cur_time = int(words[4])
        if cur_time>end_ts: 
            #统计当前10s内用户写数据大小
            total=0
            for value in my_dict.values():
                total+=value
            
            initial_size+=total
            user_space.append(initial_size)
            #存储时间戳和用户数据大小
            # print(initial_size,words[4])
            f.write(str(initial_size)+' '+words[4]+'\n')
            #处理下一个10s间隔
            my_dict.clear()
            start_ts=int(words[4])
            end_ts=start_ts+1.0e7
            
        if(words[1]==1):
            cur_key=words[0]
            cur_value_size =words[3]
            #更新cur_key的存储空间
            if cur_key in global_dict :
                initial_size-=global_dict[cur_key]
            global_dict[cur_key]=cur_value_size
            my_dict[cur_key] = cur_value_size

with open(output_storage, 'w') as f:
    lines=f.readlines()
    i=0
    for line in lines:
        words=line.split('\t')
        storage_space = words[0]
        space_amplipication = float(storage_space/user_space[i])
        i+=1
        
