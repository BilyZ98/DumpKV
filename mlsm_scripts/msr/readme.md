
1. Files

This distribution contains the following files:
     a) README.txt: this file
     b) *.csv.gz: I/O trace files, described below
     c) MD5.txt: output of md5sum for csv.gz files
     d) DISCLAIMER.txt: Disclaimer applying to all files in distribution

2. I/O trace files

There are 36 I/O traces from 36 different volumes on 13 servers.  More
information about these is available from the FAST 2008 paper (see
below).

Each trace file is named as <hostname>_<disknumber>.csv.gz. The hostnames
are "friendly" names corresponding to the names used in the paper. Disk
numbers are logical block device numbers.

Generally, disk 0 corresponds to the system or boot disk. However, for
the host "src1", disk 2 is the system disk.

3. I/O trace file format

The files are gzipped csv (comma-separated text) files. The fields in
the csv are:

Timestamp,Hostname,DiskNumber,Type,Offset,Size,ResponseTime

Timestamp is the time the I/O was issued in "Windows filetime"
Hostname is the hostname (should be the same as that in the trace file name)
DiskNumber is the disknumber (should be the same as in the trace file name)
Type is "Read" or "Write"
Offset is the starting offset of the I/O in bytes from the start of the logical
disk.
Size is the transfer size of the I/O request in bytes.
ResponseTime is the time taken by the I/O to complete, in Windows filetime
units.

4. Attribution.

Please cite the following publication as a reference in any published
work using these traces.

Write Off-Loading: Practical Power Management for Enterprise Storage
Dushyanth Narayanan, Austin Donnelly, and Antony Rowstron
Microsoft Research Ltd. 
Proc. 6th USENIX Conference on File and Storage Technologies (FAST  08)
http://www.usenix.org/event/fast08/tech/narayanan.html

