// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "cudakernel/memory/slice.h"
#include "cudakernel/common/divmod_fast.h"
#include "cudakernel/common/memory_utils.h"
#include "cudakernel/common/common.h"
#include "ppl/common/tensor_shape.h"
#include "ppl/common/retcode.h"
#include <cuda_runtime.h>

#define MAX_DIM_SIZE SLICE_PARAM_MAX_DIM_SIZE
template <typename T>
__global__ void ppl_cukernel_slice(
    int64_t num_elems,
    int num_dims,
    SliceKernelParam param,
    GArray<int64_t> input_strides,
    const T* input,
    GArray<int64_t> output_strides,
    GArray<DivModFast> output_strides_fast,
    T* output)
{
    int64_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= num_elems)
        return;
    int output_idx[MAX_DIM_SIZE];
    int input_idx[MAX_DIM_SIZE];
    int idx, remain = index;
    for (int it = 0; it < num_dims; ++it) {
        output_strides_fast[it].divmod(remain, idx, remain);
        output_idx[it] = idx;
    }

    // copy output_idx to input_idx
    for (int it = 0; it < num_dims; ++it)
        input_idx[it] = output_idx[it];

    // calc input_idx according to axes[]
    for (int it = 0; it < param.axes_num; ++it) {
        int axis        = param.axes[it];
        input_idx[axis] = output_idx[axis] * param.steps[it] + param.starts[it];
    }

    int64_t input_offset  = 0;
    int64_t output_offset = 0;
    for (int it = 0; it < num_dims; ++it) {
        input_offset += input_idx[it] * input_strides[it];
        output_offset += output_idx[it] * output_strides[it];
    }
    output[output_offset] = input[input_offset];
}

bool isFastSliceSupported(const SliceKernelParam& param, int32_t num_dims) {
    if (num_dims != 4) return false;
    if (param.axes_num > 2) return false;
    for (int32_t i = 0; i < param.axes_num; ++i) {
        int axis        = param.axes[i];
        if (axis < (num_dims - 2)) return false;
        if (param.steps[i] != 1) return false;
        if (param.starts[i] != 0) return false;
    }
    return true;
}

template <typename T>
__global__ void ppl_cukernel_slice_fast_ndarray(const T* input, int src_height, int src_width,
    T* output, int dst_height, int dst_width) {
    int dst_hgt = blockIdx.y * blockDim.y + threadIdx.y;
    int dst_wdt = blockIdx.x * blockDim.x + threadIdx.x;
    if (dst_hgt >= dst_height || dst_wdt >= dst_width) return;
    int b_idx = blockIdx.z;
    int dst_idx = b_idx * dst_height * dst_width + dst_hgt * dst_width + dst_wdt;
    int src_idx = b_idx * src_height * src_width + dst_hgt * src_width + dst_wdt;
    output[dst_idx] = input[src_idx];
}

template <typename T>
__global__ void ppl_cukernel_slice_fast_nhwc(int channel, const T* input, int src_height, int src_width,
    int src_pad_channel, T* output, int dst_height, int dst_width, int dst_pad_channel) {
    int dst_hw_idx = blockIdx.y * blockDim.y + threadIdx.y;
    int dst_hgt = dst_hw_idx / dst_width;
    int dst_wdt = dst_hw_idx % dst_width;
    int chl_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (chl_idx >= channel || dst_hgt >= dst_height) return;
    int b_idx = blockIdx.z;
    int dst_idx = (b_idx * dst_height * dst_width + dst_hgt * dst_width + dst_wdt) * dst_pad_channel + chl_idx;
    int src_idx = (b_idx * src_height * src_width + dst_hgt * src_width + dst_wdt) * src_pad_channel + chl_idx;
    output[dst_idx] = input[src_idx];
}

ppl::common::RetCode PPLCUDASliceForwardImp(
    cudaStream_t stream,
    SliceKernelParam param,
    const ppl::common::TensorShape* input_shape,
    const void* input,
    ppl::common::TensorShape* output_shape,
    void* output)
{
    if (output_shape->CalcElementsIncludingPadding() == 0)
        return ppl::common::RC_SUCCESS;
    int num_dims       = output_shape->GetDimCount();
    if (isFastSliceSupported(param, num_dims)) {
        #define SWITCH_CASE(TYPE) \
        case sizeof(TYPE): { \
            if (output_shape->GetDataFormat() == ppl::common::DATAFORMAT_NDARRAY) { \
                int batch = input_shape->CalcElementsToDimensionExcludingPadding(num_dims - 2); \
                int dst_height = output_shape->GetDim(num_dims - 2); \
                int dst_width  = output_shape->GetDim(num_dims - 1); \
                int src_height = input_shape->GetDim(num_dims - 2); \
                int src_width  = input_shape->GetDim(num_dims - 1); \
                dim3 block_size(16, 16, 1); \
                dim3 grid_size(DivUp(dst_width, 16), DivUp(dst_height, 16), batch); \
                ppl_cukernel_slice_fast_ndarray<<<grid_size, block_size, 0, stream>>>( \
                    (const TYPE*)input, src_height, src_width, (TYPE*)output, dst_height, dst_width); \
                return ppl::common::RC_SUCCESS; \
            } else if (output_shape->GetDataFormat() == ppl::common::DATAFORMAT_NHWC8 || \
                        output_shape->GetDataFormat() == ppl::common::DATAFORMAT_NHWC16) { \
                int batch = input_shape->GetDim(0); \
                int channel = input_shape->GetDim(1); \
                int src_pad_channel = input_shape->GetDim(1) + input_shape->GetPadding0(1) + input_shape->GetPadding1(1); \
                int dst_pad_channel = output_shape->GetDim(1) + output_shape->GetPadding0(1) + output_shape->GetPadding1(1); \
                int dst_height = output_shape->GetDim(num_dims - 2); \
                int dst_width  = output_shape->GetDim(num_dims - 1); \
                int src_height = input_shape->GetDim(num_dims - 2); \
                int src_width  = input_shape->GetDim(num_dims - 1); \
                dim3 block_size(16, 16, 1); \
                dim3 grid_size(DivUp(channel, 16), DivUp(dst_height * dst_width, 16), batch); \
                ppl_cukernel_slice_fast_nhwc<<<grid_size, block_size, 0, stream>>>(channel, \
                    (const TYPE*)input, src_height, src_width, src_pad_channel, (TYPE*)output, dst_height, dst_width, dst_pad_channel); \
                return ppl::common::RC_SUCCESS; \
            } \
        }

        switch (ppl::common::GetSizeOfDataType(input_shape->GetDataType())) {
            SWITCH_CASE(int8_t);
            SWITCH_CASE(int16_t);
            SWITCH_CASE(int32_t);
            SWITCH_CASE(int64_t);
            default:
                return ppl::common::RC_UNSUPPORTED;
        }
        #undef SWITCH_CASE
    }
    int block_size     = 256;
    uint64_t num_elems = output_shape->CalcElementsExcludingPadding();
    int grid_size      = (num_elems + block_size - 1) / block_size;
    GArray<int64_t> input_strides(num_dims);
    GArray<int64_t> output_strides(num_dims);
    GArray<DivModFast> output_strides_fast(num_dims);
    int64_t acc_output_stride = 1;
    int64_t acc_input_stride  = 1;
    for (int it = num_dims - 1; it >= 0; --it) {
        input_strides[it]       = acc_input_stride;
        output_strides[it]      = acc_output_stride;
        output_strides_fast[it] = DivModFast(acc_output_stride);
        acc_input_stride *= input_shape->GetDim(it);
        acc_output_stride *= output_shape->GetDim(it);
    }
    if (output_shape->GetDataFormat() == ppl::common::DATAFORMAT_NHWC8 ||
        output_shape->GetDataFormat() == ppl::common::DATAFORMAT_NHWC16) {
        acc_output_stride = 1;
        acc_input_stride  = 1;
        for (int it = num_dims - 1; it >= 0; --it) {
            if (it == num_dims - 1) {
                input_strides[1]  = acc_input_stride;
                output_strides[1] = acc_output_stride;
                acc_input_stride *= input_shape->GetDim(1) + input_shape->GetPadding0(1) + input_shape->GetPadding1(1);
                acc_output_stride *= output_shape->GetDim(1) + output_shape->GetPadding0(1) + output_shape->GetPadding1(1);
            } else if (it == 0) {
                input_strides[it]  = acc_input_stride;
                output_strides[it] = acc_output_stride;
                acc_input_stride *= input_shape->GetDim(it);
                acc_output_stride *= output_shape->GetDim(it);
            } else {
                input_strides[it + 1]  = acc_input_stride;
                output_strides[it + 1] = acc_output_stride;
                acc_input_stride *= input_shape->GetDim(it + 1);
                acc_output_stride *= output_shape->GetDim(it + 1);
            }
        }
    }

#define SWITCH_CASE(TYPE)                                                                                                       \
    case sizeof(TYPE): {                                                                                                        \
        ppl_cukernel_slice<<<grid_size, block_size, 0, stream>>>(                                                               \
            num_elems, num_dims, param, input_strides, (const TYPE*)input, output_strides, output_strides_fast, (TYPE*)output); \
        return ppl::common::RC_SUCCESS;                                                                                         \
    }
    switch (ppl::common::GetSizeOfDataType(input_shape->GetDataType())) {
        SWITCH_CASE(int8_t);
        SWITCH_CASE(int16_t);
        SWITCH_CASE(int32_t);
        SWITCH_CASE(int64_t);
        default:
            return ppl::common::RC_UNSUPPORTED;
    }
#undef SWITCH_CASE
}
