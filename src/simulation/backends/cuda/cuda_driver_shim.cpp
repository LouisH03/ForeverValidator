#include <cuda.h>
#include <cuda_runtime_api.h>

#include <cstddef>

namespace {

template <typename Function>
Function
ResolveDriverFunction(const char *symbol,
                      unsigned long long flags = cudaEnableDefault) noexcept {
  void *entryPoint = nullptr;
  cudaDriverEntryPointQueryResult queryResult =
      cudaDriverEntryPointSymbolNotFound;
  const cudaError_t result = cudaGetDriverEntryPointByVersion(
      symbol, &entryPoint, CUDART_VERSION, flags, &queryResult);
  if (result != cudaSuccess || queryResult != cudaDriverEntryPointSuccess ||
      entryPoint == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<Function>(entryPoint);
}

constexpr CUresult DriverUnavailable() noexcept {
  return CUDA_ERROR_NOT_INITIALIZED;
}

} // namespace

extern "C" CUresult CUDAAPI cuFuncGetAttribute(int *value,
                                               CUfunction_attribute attribute,
                                               CUfunction function) {
  using Function = CUresult(CUDAAPI *)(int *, CUfunction_attribute, CUfunction);
  static Function entry = ResolveDriverFunction<Function>("cuFuncGetAttribute");
  return entry == nullptr ? DriverUnavailable()
                          : entry(value, attribute, function);
}

extern "C" CUresult CUDAAPI cuFuncGetName(const char **name,
                                          CUfunction function) {
  using Function = CUresult(CUDAAPI *)(const char **, CUfunction);
  static Function entry = ResolveDriverFunction<Function>("cuFuncGetName");
  return entry == nullptr ? DriverUnavailable() : entry(name, function);
}

extern "C" CUresult CUDAAPI cuGetErrorString(CUresult error,
                                             const char **message) {
  using Function = CUresult(CUDAAPI *)(CUresult, const char **);
  static Function entry = ResolveDriverFunction<Function>("cuGetErrorString");
  if (entry != nullptr) {
    return entry(error, message);
  }
  if (message != nullptr) {
    *message = "CUDA driver is unavailable";
  }
  return CUDA_SUCCESS;
}

extern "C" CUresult CUDAAPI cuLaunchKernel(
    CUfunction function, unsigned int gridDimX, unsigned int gridDimY,
    unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY,
    unsigned int blockDimZ, unsigned int sharedMemoryBytes, CUstream stream,
    void **kernelParameters, void **extra) {
  using Function = CUresult(CUDAAPI *)(
      CUfunction, unsigned int, unsigned int, unsigned int, unsigned int,
      unsigned int, unsigned int, unsigned int, CUstream, void **, void **);
  static Function entry =
      ResolveDriverFunction<Function>("cuLaunchKernel", cudaEnableLegacyStream);
  return entry == nullptr
             ? DriverUnavailable()
             : entry(function, gridDimX, gridDimY, gridDimZ, blockDimX,
                     blockDimY, blockDimZ, sharedMemoryBytes, stream,
                     kernelParameters, extra);
}

extern "C" CUresult CUDAAPI cuModuleEnumerateFunctions(
    CUfunction *functions, unsigned int functionCount, CUmodule module) {
  using Function = CUresult(CUDAAPI *)(CUfunction *, unsigned int, CUmodule);
  static Function entry =
      ResolveDriverFunction<Function>("cuModuleEnumerateFunctions");
  return entry == nullptr ? DriverUnavailable()
                          : entry(functions, functionCount, module);
}

extern "C" CUresult CUDAAPI
cuModuleGetFunctionCount(unsigned int *functionCount, CUmodule module) {
  using Function = CUresult(CUDAAPI *)(unsigned int *, CUmodule);
  static Function entry =
      ResolveDriverFunction<Function>("cuModuleGetFunctionCount");
  return entry == nullptr ? DriverUnavailable() : entry(functionCount, module);
}

extern "C" CUresult CUDAAPI cuModuleLoadData(CUmodule *module,
                                             const void *image) {
  using Function = CUresult(CUDAAPI *)(CUmodule *, const void *);
  static Function entry = ResolveDriverFunction<Function>("cuModuleLoadData");
  return entry == nullptr ? DriverUnavailable() : entry(module, image);
}

extern "C" CUresult CUDAAPI cuModuleUnload(CUmodule module) {
  using Function = CUresult(CUDAAPI *)(CUmodule);
  static Function entry = ResolveDriverFunction<Function>("cuModuleUnload");
  return entry == nullptr ? DriverUnavailable() : entry(module);
}

extern "C" CUresult CUDAAPI cuOccupancyMaxActiveBlocksPerMultiprocessor(
    int *blockCount, CUfunction function, int blockSize,
    std::size_t dynamicSharedMemoryBytes) {
  using Function = CUresult(CUDAAPI *)(int *, CUfunction, int, std::size_t);
  static Function entry = ResolveDriverFunction<Function>(
      "cuOccupancyMaxActiveBlocksPerMultiprocessor");
  return entry == nullptr
             ? DriverUnavailable()
             : entry(blockCount, function, blockSize, dynamicSharedMemoryBytes);
}
