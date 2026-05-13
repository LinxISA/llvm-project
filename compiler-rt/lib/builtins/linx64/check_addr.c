extern void SRE_printf(const char *format, ...);
void checkMemlegality(void *StoreAddr, long StoreSize, void *LoadAddr,
                      long LoadSize, const char *FunctionName,
                      const char *StoreFile, const char *StoreDir,
                      int StoreLine, int StoreCol) {
  if ((StoreAddr + StoreSize) <= LoadAddr ||
      (LoadAddr + LoadSize) <= StoreAddr) {
    return;
  } else {
    SRE_printf("ERROR: Memory address conflict in function '%s'.\n"
               "Store Source Location: %s/%s, Line %d, Column %d\n",
               FunctionName, StoreDir, StoreFile, StoreLine, StoreCol);
    SRE_printf("StoreAddr: %p, StoreSize: %ld, LoadAddr: %p, LoadSize: %ld.\n",
               StoreAddr, StoreSize, LoadAddr, LoadSize);
  }
  return;
}

void checkIRMemlegality(void *StoreAddr, long StoreSize, void *LoadAddr,
                        long LoadSize, const char *FunctionName,
                        const char *LoadDumpStr, const char *StoreDumpStr) {
  if ((StoreAddr + StoreSize) <= LoadAddr ||
      (LoadAddr + LoadSize) <= StoreAddr) {
    return;
  } else {
    SRE_printf("ERROR: Memory address conflict in function '%s'.\n"
               "Store instruction dump info: %s.\n"
               "Load instruction dump info: %s.\n"
               "StoreAddr: %p, StoreSize: %ld, LoadAddr: %p, LoadSize: %ld.\n",
               FunctionName, StoreDumpStr, LoadDumpStr, StoreAddr, StoreSize,
               LoadAddr, LoadSize);
  }
  return;
}