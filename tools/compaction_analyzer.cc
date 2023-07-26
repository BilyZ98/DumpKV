#ifndef GFLAGS
#include <cstdio>
int main() {
  fprintf(stderr, "Please install gflags to run rocksdb tools\n");
  return 1;
}
#else
#include "tools/compaction_tracer_analyzer_tool.h"
int main(int argc, char** argv) {
  return ROCKSDB_NAMESPACE::compaction_tracer_analyzer_main(argc, argv);
}
#endif
