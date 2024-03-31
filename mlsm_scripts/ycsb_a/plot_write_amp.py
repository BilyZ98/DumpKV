from textwrap import wrap
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os
import csv

# 1. Read the data from the file
log_dir="./log_output_4096_uniform"

large_x = []
large_y = []
small_x = []
small_y = []

large_ops_per_sec = []
small_ops_per_sec = []

large_total_size = []
small_total_size = []
for root, dirs, files in os.walk(log_dir):
    for file in files:
        file_path = os.path.join(root, file)
        print('fle_path',file_path)
        if file.endswith(".tsv"):
            print(file)
            data = pd.read_csv(file_path, sep='\t', skiprows=1, header=None)
            lsm_size = data.iloc[0,4]
            lsm_size = lsm_size.replace("GB", "")
            total_blob_size = data.iloc[0,5]
            total_blob_size = total_blob_size.replace("GB", "")

            total_size = float(lsm_size) + float(total_blob_size)

            if data.iloc[0,1].find("small") != -1:
                small_x.append(data.iloc[0,1])
                small_y.append(float(data.iloc[0,3]))
                small_ops_per_sec.append(float(data.iloc[0,6]))
                # remove GB from the size, exmpale 1.6GB
                small_total_size.append(total_size)

            else:
                large_x.append(data.iloc[0,1])
                large_y.append(float(data.iloc[0,3]))
                large_ops_per_sec.append(float(data.iloc[0,6]))
                large_total_size.append(total_size)

            # x.append(data.iloc[0,1])
            # y.append(float(data.iloc[0,3]))
            # ops_per_sec.append(float(data.iloc[0,6]))

    
# plt.xticks(rotation=90)

plt.figure(figsize=(30,30))
# set_figwidth(50)# Labeling the axes
plt.xlabel('Testname')
plt.ylabel('Wamp')
plt.title('WAMP')

large_labels_wrapped = ['\n'.join(wrap(l, 40)) for l in large_x]
plt.barh(large_labels_wrapped, large_y ) 
# plt.bar(labels_wrapped, y, width=0.5)
# plt.setp(ax.set_yticklabels(labels_wrapped))
# plt.tight_layout()
# plt.yticks(15 * np.arange(len(x)), labels_wrapped)
save_fig_name="large_wamp.png"
plt.savefig(save_fig_name)

plt.figure(figsize=(30,30))
# set_figwidth(50)# Labeling the axes
plt.xlabel('Testname')
plt.ylabel('Wamp')
plt.title('WAMP')
small_labels_wrapped = ['\n'.join(wrap(l, 40)) for l in small_x]
plt.barh(small_labels_wrapped, small_y )
save_fig_name="small_wamp.png"
plt.savefig(save_fig_name)


plt.figure(figsize=(30,20))
plt.xlabel('ops_per_sec')
plt.ylabel('Testname')
plt.title('ops_per_sec')
plt.barh(large_labels_wrapped, large_ops_per_sec )
save_fig_name="large_ops_per_sec.png"
plt.savefig(save_fig_name)

plt.figure(figsize=(30,20))
plt.xlabel('ops_per_sec')
plt.ylabel('Testname')
plt.title('ops_per_sec')
plt.barh(small_labels_wrapped, small_ops_per_sec )
save_fig_name="small_ops_per_sec.png"
plt.savefig(save_fig_name)

plt.figure(figsize=(30,20))
plt.xlabel('Total Size')
plt.ylabel('Testname')
plt.title('Total Size')
plt.barh(large_labels_wrapped, large_total_size )
save_fig_name="large_total_size.png"
plt.savefig(save_fig_name)

plt.figure(figsize=(30,20))
plt.xlabel('Total Size')
plt.ylabel('Testname')
plt.title('Total Size')
plt.barh(small_labels_wrapped, small_total_size )
save_fig_name="small_total_size.png"
plt.savefig(save_fig_name)
