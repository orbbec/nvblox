/*
Copyright 2023 NVIDIA CORPORATION

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "nvblox/core/cuda_stream.h"

#include "glog/logging.h"

#include "nvblox/core/internal/error_check.h"

namespace nvblox {

void CudaStream::synchronize() const {
  checkCudaErrors(cudaStreamSynchronize(*stream_ptr_));
}

CudaStreamOwning::CudaStreamOwning(const unsigned int flags)
    : CudaStream(&stream_) {
  checkCudaErrors(cudaStreamCreateWithFlags(&stream_, flags));
}

CudaStreamOwning::~CudaStreamOwning() {
  this->synchronize();
  checkCudaErrors(cudaStreamDestroy(stream_));
}

}  // namespace nvblox
