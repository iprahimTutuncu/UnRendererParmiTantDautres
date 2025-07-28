#pragma once
#include <iostream>

namespace {
    static inline void print_props(cudaDeviceProp const& props) {
        std::cout << "Nom du GPU: " << props.name
                  << "\nMémoire globale: " << props.totalGlobalMem << " bytes"
                  << "\nMémoire constante: " << props.totalConstMem << " bytes"
                  << "\nMémoire partagée par bloc: " << props.sharedMemPerBlock << " bytes"
                  << "\nNombre de registres par bloc: " << props.regsPerBlock
                  << "\nNombre de multiprocesseurs: " << props.multiProcessorCount
                  << "\nFréquence du GPU: " << props.clockRate << " kHz"
                  << "\nTaille du warp: " << props.warpSize
                  << "\nTaille maximale de grille: " << props.maxGridSize[0] << " x " << props.maxGridSize[1] << " x " << props.maxGridSize[2]
                  << "\nTaille maximale de bloc: " << props.maxThreadsDim[0] << " x " << props.maxThreadsDim[1] << " x " << props.maxThreadsDim[2]
                  << "\nNombre de threads par bloc: " << props.maxThreadsPerBlock
                  << "\nNombre de blocs par multiprocesseur: " << props.maxBlocksPerMultiProcessor
                  << "\nVersion de CUDA: " << props.major << "." << props.minor
                  << "\nVersion de l'API: " << props.major << "." << props.minor
                  << "\nPrise en charge de l'API de thread: " << (props.concurrentKernels ? "true" : "false") << std::endl;
    }
}

static inline void print_cuda_devices_info() {
    int nbgpu;
    cudaDeviceProp prop;
    cudaGetDeviceCount(&nbgpu);
    for (int i = 0; i < nbgpu; i++) {
        cudaGetDeviceProperties(&prop, i);
        print_props(prop);
    }
}

inline cudaError_t checkCuda(cudaError_t result) {
#if defined(NDEBUG) || defined(_DEBUG)
    if (result != cudaSuccess) {
        fprintf(stderr, "CUDA Runtime Error: %s\n",
            cudaGetErrorString(result));
        assert(result == cudaSuccess);
    }
#endif
    return result;
}
