// RUN: iree-opt -split-input-file -iree-convert-hal-to-vm %s | IreeFileCheck %s

// CHECK-LABEL: @allocatorComputeSize
func @allocatorComputeSize(%arg0 : !iree.ref<!hal.allocator>) -> i32 {
  %c1024_i32 = constant 1024 : i32
  // CHECK: %0 = vm.call.variadic @hal.allocator.compute_size(%arg0, %c6, %c15, [%c1024, %c1024], %c4) : (!iree.ref<!hal.allocator>, i32, i32, i32..., i32) -> i32
  %0 = hal.allocator.compute_size %arg0, "HostLocal", "All", shape=[%c1024_i32, %c1024_i32], element_size=4
  return %0 : i32
}

// -----

// CHECK-LABEL: @allocatorAllocate
func @allocatorAllocate(%arg0 : !iree.ref<!hal.allocator>) -> !iree.ref<!hal.buffer> {
  %c1024_i32 = constant 1024 : i32
  // CHECK: %ref = vm.call @hal.allocator.allocate(%arg0, %c6, %c15, %c1024) : (!iree.ref<!hal.allocator>, i32, i32, i32) -> !iree.ref<!hal.buffer>
  %0 = hal.allocator.allocate %arg0, "HostLocal", "All", %c1024_i32 : !iree.ref<!hal.buffer>
  return %0 : !iree.ref<!hal.buffer>
}

// -----

// CHECK: vm.rodata @allocatorAllocateConst_const_0 dense<123> : tensor<4x4xi32>
// CHECK-LABEL: func @allocatorAllocateConst
func @allocatorAllocateConst() -> !iree.ref<!hal.buffer> {
  %allocator = "test_hal.allocator"() : () -> !iree.ref<!hal.allocator>
  // CHECK:  %allocatorAllocateConst_const_0 = vm.const.ref.rodata @allocatorAllocateConst_const_0 : !iree.byte_buffer_ref
  // CHECK: %ref = vm.call.variadic @hal.allocator.allocate.const(%0, %c6, %c2, [%c4, %c4_0], %c4_1, %allocatorAllocateConst_const_0) : (!iree.ref<!hal.allocator>, i32, i32, i32..., i32, !iree.byte_buffer_ref) -> !iree.ref<!hal.buffer>
  %buffer = hal.allocator.allocate.const %allocator, "HostVisible|HostCoherent", "Transfer" : !iree.ref<!hal.buffer> = dense<123> : tensor<4x4xi32>
  return %buffer : !iree.ref<!hal.buffer>
}

// -----

// CHECK-LABEL: @allocatorAllocateShaped
func @allocatorAllocateShaped(%arg0 : !iree.ref<!hal.allocator>) -> !iree.ref<!hal.buffer> {
  %c1024_i32 = constant 1024 : i32
  // CHECK: %ref = vm.call.variadic @hal.allocator.allocate.shaped(%arg0, %c6, %c15, [%c1024, %c1024], %c4) : (!iree.ref<!hal.allocator>, i32, i32, i32..., i32) -> !iree.ref<!hal.buffer>
  %0 = hal.allocator.allocate.shaped %arg0, "HostLocal", "All", shape=[%c1024_i32, %c1024_i32], element_size=4 : !iree.ref<!hal.buffer>
  return %0 : !iree.ref<!hal.buffer>
}
