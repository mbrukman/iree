// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iree/compiler/Dialect/IREE/IR/IREETypes.h"

#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/StandardTypes.h"
#include "mlir/IR/TypeSupport.h"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace detail {

struct RefPtrTypeStorage : public TypeStorage {
  RefPtrTypeStorage(Type objectType, unsigned subclassData = 0)
      : TypeStorage(subclassData),
        objectType(objectType.cast<RefObjectType>()) {}

  /// The hash key used for uniquing.
  using KeyTy = Type;
  bool operator==(const KeyTy &key) const { return key == objectType; }

  static RefPtrTypeStorage *construct(TypeStorageAllocator &allocator,
                                      const KeyTy &key) {
    // Initialize the memory using placement new.
    return new (allocator.allocate<RefPtrTypeStorage>()) RefPtrTypeStorage(key);
  }

  RefObjectType objectType;
};

}  // namespace detail
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir

using namespace mlir;
using namespace mlir::iree_compiler::IREE;

//===----------------------------------------------------------------------===//
// RefPtrType
//===----------------------------------------------------------------------===//

RefPtrType RefPtrType::get(RefObjectType objectType) {
  return Base::get(objectType.getContext(), TypeKind::RefPtr, objectType);
}

RefPtrType RefPtrType::getChecked(Type objectType, Location location) {
  return Base::getChecked(location, objectType.getContext(), TypeKind::RefPtr,
                          objectType);
}

RefObjectType RefPtrType::getObjectType() { return getImpl()->objectType; }
