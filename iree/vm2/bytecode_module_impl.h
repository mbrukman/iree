// Copyright 2019 Google LLC
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

#ifndef IREE_VM_BYTECODE_MODULE_IMPL_H_
#define IREE_VM_BYTECODE_MODULE_IMPL_H_

#include <stdint.h>

#include "iree/base/api.h"
#include "iree/vm2/module.h"
#include "iree/vm2/ref.h"
#include "iree/vm2/stack.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Matches the FunctionDescriptor struct in the flatbuffer.
typedef struct {
  int32_t bytecode_offset;
  int32_t bytecode_length;
  int8_t i32_register_count;
  int8_t ref_register_count;
} iree_vm_function_descriptor_t;

// A loaded bytecode module.
typedef struct {
  // Interface routing to the bytecode module functions.
  // Must be first in the struct as we dereference the interface to find our
  // members below.
  iree_vm_module_t interface;

  // Table of internal function bytecode descriptors.
  // Mapped 1:1 with internal functions. Each defined bytecode span represents a
  // range of bytes in |bytecode_data|.
  int32_t function_descriptor_count;
  const iree_vm_function_descriptor_t* function_descriptor_table;
  // A pointer to the bytecode data embedded within the module.
  iree_const_byte_span_t bytecode_data;

  // Allocator this module was allocated with and must be freed with.
  iree_allocator_t allocator;

  // Underlying FlatBuffer data and allocator (which may be null).
  iree_const_byte_span_t flatbuffer_data;
  iree_allocator_t flatbuffer_allocator;
} iree_vm_bytecode_module_t;

// Per-instance module state.
// This is allocated with a provided allocator as a single flat allocation.
// This struct is a prefix to the allocation pointing into the dynamic offsets
// of the allocation storage.
typedef struct {
  // Combined rwdata storage for the entire module, including globals.
  // Aligned to 16 bytes (128-bits) for SIMD usage.
  iree_byte_span_t rwdata_storage;

  // Global i32 storage, aligned to 16 bytes (128-bits) for SIMD usage.
  int32_t* global_i32_table;

  // Global ref_ptr values, indexed by global ordinal.
  int32_t global_ref_count;
  iree_vm_ref_t* global_ref_table;

  // Initialized references to rodata segments.
  // Right now these don't do much, however we can perform lazy caching and
  // on-the-fly decompression using this information.
  int32_t rodata_ref_count;
  iree_vm_const_buffer_ref_t* rodata_ref_table;

  // Resolved function imports.
  int32_t import_count;
  iree_vm_function_t* import_table;

  // Allocator used for the state itself and any runtime allocations needed.
  iree_allocator_t allocator;
} iree_vm_bytecode_module_state_t;

// Begins (or resumes) execution of the given |entry_frame| and continues until
// either a yield or return. |out_result| will contain the result status for
// continuation, if needed.
iree_status_t iree_vm_bytecode_dispatch(
    iree_vm_bytecode_module_t* module,
    iree_vm_bytecode_module_state_t* module_state, iree_vm_stack_t* stack,
    iree_vm_stack_frame_t* entry_frame, iree_vm_execution_result_t* out_result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_VM_BYTECODE_MODULE_IMPL_H_