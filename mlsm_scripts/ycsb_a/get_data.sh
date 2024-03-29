#!/usr/bin/env bash
#
# Size Constants
K=1024
M=$((1024 * K))
G=$((1024 * M))
T=$((1024 * G))

output_dir="./log_output_4096"
if [ ! -d "$output_dir" ]; then
  mkdir -p "$output_dir"
fi

function get_data {
  test_out=$1
  test_name=$2
  bench_name="ycsb_a"

  if [ ! -f "$test_out" ]; then
    echo "File $test_out does not exist"
    return
  fi

  bench_str="ycsb_a       :"
  if ! grep ^"$bench_str" "$test_out" > /dev/null 2>&1 ; then
    echo -e "failed\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t$test_name$"
    return
  fi


  report="$output_dir/${test_name}_report.tsv"

  date=$( grep ^Date: $test_out | awk '{ print $6 "-" $3 "-" $4 "T" $5 }' )

  uptime=$( grep ^Uptime\(secs $test_out | tail -1 | awk '{ printf "%.0f", $2 }' )

  wamp=$( grep "^ Sum" "$test_out" | tail -1 | awk '{ printf "%.1f", $12}' )

  # blob_total_size=$()
  lsm_size=$( grep "^ Sum" "$test_out" | tail -1 | awk '{ printf "%.1f%s", $3, $4 }' )
  blob_size=$( grep "^Blob file count:" "$test_out" | tail -1 | awk '{ printf "%.1f%s", $7, $8 }' )
  blob_size="${blob_size/,/}"

  ops_sec=$( grep ^"$bench_str" "$test_out" | awk '{ print $5 }' )
  usecs_op=$( grep ^"$bench_str" "$test_out" | awk '{ printf "%.1f", $3 }' )

  echo "ops_sec is $ops_sec"
  echo "usecs_op is $usecs_op"

  echo -e "Date\tTest\tUptime\tWAMP\tLSM Size\tBlob Size\tOPS_SEC\tUSECS_OP" > "$report"
  echo -e "$date\t$test_name\t$uptime\t$wamp\t$lsm_size\t$blob_size\t$ops_sec\t$usecs_op" >> "$report"
  # if [ -f "$report" ]; then
  #   echo -e "$date\t$test_name\t$uptime\t$wamp\t$lsm_size\t$blob_size" >> "$report"
  # else
  #   echo -e "Date\tTest\tUptime\tWAMP\tLSM Size\tBlob Size" > "$report"
  #   echo -e "$date\t$test_name\t$uptime\t$wamp\t$lsm_size\t$blob_size" >> "$report"
  # fi  

  echo "report file name is ${report}"
}

function call_get_data {

for i in "${!log_files_names[@]}"; do
  log_file="${log_files_names[$i]}"
  # test_name="${test_names[$i]}"
  test_name="${log_file}"
  echo "log file is ${log_file}"
  echo "test name is ${test_name}"
  echo "src dir is ${src_dir}"
  
  # cp "${src_dir}/${log_file}" "${output_dir}"
  src_dir_name="${src_dir}/${log_file}"
  # get file ends with .txt
  src_file_path=$(find "${src_dir_name}" -type f -name "*.txt")
  echo "src file path is ${src_file_path}"
  # for src_file_name in *.txt ; do
  #   echo "src file name is ${src_file_name}"
  #   break
    # if [[ $src_file_name != *.txt ]]; then
    #   continue
    # echo "src file name is ${src_file_name}"
    # else
    # echo "src file name is ${src_file_name}"
    # break
    # fi
    #
  get_data "$src_file_path" "${test_name}"
done

}



# 2024-03-26-11-37-02-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_2_1024
# 2024-03-27-03-55-18-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_1_1024
# 2024-03-27-08-03-54-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_0_1024
# 2024-03-26-15-37-14-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_2_1024
# 2024-03-27-04-48-03-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_1_1024
# 2024-03-27-13-16-42-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_0_1024

log_files_names=(
2024-03-26-11-37-02-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_2_1024
2024-03-27-08-03-54-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_0_1024
2024-03-27-14-43-19-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_1_1024
2024-03-26-15-37-14-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_2_1024
2024-03-27-04-48-03-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_1_1024
2024-03-27-14-44-24-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_0_1024

)

log_files_names=(
2024-03-26-13-49-00-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_2_4096
2024-03-27-04-45-50-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_1_4096
2024-03-27-10-26-40-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-large-sst-file-release-default_0_4096
2024-03-26-19-55-00-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_2_4096
2024-03-27-07-28-11-more_prediction-0.2-zipfian-100M-ycsba-no-train-data-log-only-write-no-blob-cache-blob-file-starting-level1-no-paranoid-file-checks-small-sst-file-release-default_1_4096
)

test_names=(
  "large-sst-file-release-default_lifetimeidx_2" 
)


src_dir="/mnt/nvme/zt/rocksdb_kv_sep_delay_prediction/tools"
call_get_data 


src_dir="./ctb1_mnt/"
log_files_names=(
2024-03-26-20-20-16-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-release-default_0_1024
2024-03-26-20-27-27-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-smaller-sst-release-default_0_1024
2024-03-26-22-11-41-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-large-sst-0.2-agecutoff-release-default_0_1024
2024-03-26-22-13-37-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-small-sst-0.2-agecutoff-release-default_0_1024

)

log_files_names=(
2024-03-27-03-28-49-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-small-sst-0.2-agecutoff-release-default_0_4096
2024-03-26-23-09-17-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-smaller-sst-release-default_0_4096
2024-03-26-22-45-39-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-release-default_0_4096
2024-03-27-03-24-51-std-rocksdb-kv-sep-0.2-zipfian-100M-ycsba-large-sst-0.2-agecutoff-release-default_0_4096
)

# for i in "${!log_files_names[@]}"; do
#   log_file="${log_files_names[$i]}"
#   echo "log file is ${log_file}"
# done
# exit
call_get_data



























