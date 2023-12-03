

dir_path="/mnt/nvme0n1/workloads/ibm_cos_trace/"

pushd $dir_path
for f in *; do
  if [[ ! $f =~ \.tgz$ ]] && [[ ! $f =~ \.sh$ ]]; then
    echo "Processing $f file..."
    op_type_count=$(cut -d ' ' -f 2,2 $f | sort | uniq -c )
    echo "$op_type_count"
  fi
done

popd
