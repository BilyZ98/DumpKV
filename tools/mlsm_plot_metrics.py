from matplotlib import pyplot as plt
import numpy as np
import os
import sys

with_gc_dir = sys.argv[1] 
without_gc_filename = sys.argv[2]

write_amps = []
space_amps = []
for i in range(6):
    x = i * 0.2
    x = round(x, 1)
    with_gc_age_cutoff_name = with_gc_dir + '_' + str(x) + '/report.tsv'
    print('filename:{}'.format(with_gc_age_cutoff_name))

    with open(with_gc_age_cutoff_name) as f:
        lines = f.readlines()
        last_line = lines[-1]
        metrics = last_line.split()
        with_gc_wamp = float(metrics[5])
        with_gc_space_amp = float(metrics[-1])
        write_amps.append(with_gc_wamp)
        space_amps.append(with_gc_space_amp)


with open(without_gc_filename) as f:
    lines = f.readlines()
    last_line = lines[-1]
    metrics = last_line.split()
    without_gc_wamp = float(metrics[5])
    without_gc_space_amp = float(metrics[-1])
    write_amps.append(without_gc_wamp)
    space_amps.append(without_gc_space_amp)


print('write_amps:{}'.format(write_amps))
print('space_amps:{}'.format(space_amps))
plt.figure()
plt.plot(np.arange(7)*0.2, write_amps, label='WAMP')
plt.xticks(np.arange(7)*0.2, ('0.0', '0.2', '0.4', '0.6', '0.8', '1.0', 'without GC'))
plt.legend()
plt.savefig('wamp.png')

# loc, axs = plt.subplots(2, 1, figsize=(10, 5))
plt.figure()
plt.subplot(211)
plt.plot(np.arange(7)*0.2, write_amps, label='WAMP')
plt.xticks(np.arange(7)*0.2, ('0.0', '0.2', '0.4', '0.6', '0.8', '1.0', 'without GC'))
plt.legend()
plt.subplot(212)
plt.plot(np.arange(7)*0.2, space_amps, label='Space AMP')
plt.xticks(np.arange(7)*0.2, ('0.0', '0.2', '0.4', '0.6', '0.8', '1.0', 'without GC'))
plt.legend()
plt.savefig('metrics.png')
        


# with open(with_gc_filename) as f, open(without_gc_filename) as f_comp:
#     with_gc_lines = f.readlines()
#     last_line = with_gc_lines[-1]
#     without_gc_lines = f_comp.readlines()
#     last_line_comp = without_gc_lines[-1]
#     with_gc_metrics = last_line.split()
#     without_gc_metrics = last_line_comp.split()
#     with_gc_wamp = float(with_gc_metrics[5])
#     with_gc_space_amp = float(with_gc_metrics[-1])
#     without_gc_wamp = float(without_gc_metrics[5])
#     without_gc_space_amp = float(without_gc_metrics[-1])
#     print('with_gc_wamp:{}, with_gc_space_amp:{}, without_gc_wamp:{}, without_gc_space_amp:{}'.format(with_gc_wamp, with_gc_space_amp, without_gc_wamp, without_gc_space_amp))
#     plt.figure()
#     wamp = [with_gc_wamp, without_gc_wamp]
#     space_amp = [with_gc_space_amp, without_gc_space_amp]
#     # plot wamp and space_amp in two sub figures
#     fig, axs = plt.subplots(2, 1, figsize=(10, 5))

#     axs[0].bar(np.arange(2), wamp, width=0.2, label='WAMP')
#     axs[1].bar(np.arange(2)+0.2, space_amp, width=0.2, label='Space AMP')
#     plt.xticks(np.arange(2)+0.1, ('with GC', 'without GC'))
#     plt.legend()
#     plt.savefig('metrics.png')




