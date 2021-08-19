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

#include "ppl/nn/engines/cuda/optimizer/ops/onnx/squeeze_op.h"

#include "ppl/nn/common/logger.h"
#include "ppl/nn/engines/cuda/kernels/onnx/squeeze_kernel.h"
#include "ppl/nn/oputils/onnx/reshape_squeeze.h"

using namespace std;
using namespace ppl::common;
using namespace ppl::nn::common;

namespace ppl { namespace nn { namespace cuda {

RetCode SqueezeOp::Init(const OptKernelOptions& options) {
    auto status = GenericLoadParam<SqueezeParam>(options, &param_);
    if (status != RC_SUCCESS) {
        LOG(ERROR) << "load param failed: " << GetRetCodeStr(status);
        return status;
    }

    infer_type_func_ = [this](InputOutputInfo* info, std::vector<CudaTensorQuant>* quant, datatype_t type) -> RetCode {
        if (type == DATATYPE_UNKNOWN) {
            return InferInheritedType(info);
        } else if (type == DATATYPE_INT8) {
            auto status = CopyQuantType(info, quant);
            if (status != RC_SUCCESS) {
                LOG(ERROR) << "Set quantization for node[" << this->GetNode()->GetName() << "] failed.";
                return status;
            }
        }
        return InferDefaultType(info, type);
    };

    infer_dims_func_ = [this](InputOutputInfo* info) -> RetCode {
        return oputils::ReshapeSqueeze(info, &param_);
    };

    return RC_SUCCESS;
}

RetCode SqueezeOp::Finalize(const OptKernelOptions& options) {
    auto status = SetCommonParam(options);
    if (status != RC_SUCCESS) {
        LOG(ERROR) << "load common param failed: " << GetRetCodeStr(status);
        return status;
    }

    return RC_SUCCESS;
}

KernelImpl* SqueezeOp::CreateKernelImpl() const {
    return CreateKernelImplWithParam<SqueezeKernel>(&param_);
}

}}} // namespace ppl::nn::cuda
