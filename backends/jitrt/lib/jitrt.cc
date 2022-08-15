/*
 * Copyright 2021 The TensorFlow Runtime Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//===- jitrt.cc - ---------------------------------------------------------===//
// Support library for implementing TFRT kernels that do JIT compilation using
// MLIR framework.
//===----------------------------------------------------------------------===//

#include "tfrt/jitrt/jitrt.h"

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>

#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/IR/PassTimingInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/ExecutionEngine/CRunnerUtils.h"
#include "mlir/ExecutionEngine/OptUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Support/Timing.h"
#include "mlir/Target/LLVMIR/ModuleTranslation.h"
#include "tfrt/dtype/dtype.h"
#include "tfrt/host_context/async_value_ref.h"
#include "tfrt/jitrt/results.h"
#include "tfrt/jitrt/xla.h"
#include "tfrt/support/error_util.h"
#include "tfrt/tensor/dense_host_tensor.h"
#include "third_party/tensorflow/compiler/xla/mlir/transforms/runtime/passes.h"
#include "third_party/tensorflow/compiler/xla/mlir/transforms/runtime/specialization.h"
#include "third_party/tensorflow/compiler/xla/mlir/utils/runtime/async_runtime_api.h"
#include "third_party/tensorflow/compiler/xla/mlir/utils/runtime/constraints.h"
#include "third_party/tensorflow/compiler/xla/runtime/arguments.h"
#include "third_party/tensorflow/compiler/xla/runtime/async_runtime.h"
#include "third_party/tensorflow/compiler/xla/runtime/constraints.h"
#include "third_party/tensorflow/compiler/xla/runtime/custom_call_registry.h"
#include "third_party/tensorflow/compiler/xla/runtime/diagnostics.h"
#include "third_party/tensorflow/compiler/xla/runtime/executable.h"
#include "third_party/tensorflow/compiler/xla/runtime/execution_engine.h"
#include "third_party/tensorflow/compiler/xla/runtime/runtime.h"
#include "third_party/tensorflow/compiler/xla/runtime/symbolic_shape.h"

namespace tfrt {
namespace jitrt {

Expected<MemrefDesc> ConvertTensorToMemrefDesc(const Tensor& tensor) {
  if (auto* dht = dyn_cast<DenseHostTensor>(&tensor)) {
    return MemrefDesc(dht->shape().GetRank(), dht->dtype(),
                      const_cast<void*>(dht->data()), 0,
                      [&](auto sizes, auto strides) {
                        dht->shape().GetDimensions(sizes);
                        dht->shape().GetStrides(strides);
                      });
  }

  return MakeStringError("unsupported tensor type: ", tensor.tensor_type());
}

}  // namespace jitrt
}  // namespace tfrt
