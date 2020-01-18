// Tests printing and parsing of hal.semaphore ops.

// RUN: iree-opt -split-input-file %s | iree-opt -split-input-file | IreeFileCheck %s

// CHECK-LABEL: @semaphore_create
func @semaphore_create(%arg0 : !iree.ref<!hal.device>) -> !iree.ref<!hal.semaphore> {
  %c0 = std.constant 0 : i32
  // CHECK: %semaphore = hal.semaphore.create %arg0, %c0 : !iree.ref<!hal.semaphore>
  %semaphore = hal.semaphore.create %arg0, %c0 : !iree.ref<!hal.allocator>
  return %semaphore : !iree.ref<!hal.semaphore>
}

// -----

// CHECK-LABEL: @semaphore_status
func @semaphore_status(%arg0 : !iree.ref<!hal.semaphore>) -> i32 {
  // CHECK: = hal.semaphore.status %arg0 : i32
  %0 = hal.semaphore.status %arg0 : i32
  return %0 : i32
}

// -----

// CHECK-LABEL: @semaphore_query_value
func @semaphore_query_value(%arg0 : !iree.ref<!hal.semaphore>) {
  // CHECK: :2 = hal.semaphore.query_value %arg0 : (i32, i32)
  %0:2 = hal.semaphore.query_value %arg0 : (i32, i32)
  return
}

// -----

// CHECK-LABEL: @semaphore_signal
func @semaphore_signal(%arg0 : !iree.ref<!hal.semaphore>) {
  %c0 = std.constant 0 : i32
  // CHECK: hal.semaphore.signal %arg0, %c0
  hal.semaphore.signal %arg0, %c0
  return
}

// -----

// CHECK-LABEL: @semaphore_fail
func @semaphore_fail(%arg0 : !iree.ref<!hal.semaphore>) {
  %c0 = std.constant 0 : i32
  // CHECK: hal.semaphore.fail %arg0, %c0
  hal.semaphore.fail %arg0, %c0
  return
}

// -----

// CHECK-LABEL: @semaphore_await
func @semaphore_await(%arg0 : !iree.ref<!hal.semaphore>) {
  %c0 = std.constant 0 : i32
  // CHECK: = hal.semaphore.await %arg0 >= %c0 : i32
  %0 = hal.semaphore.await %arg0 >= %c0 : i32
  return
}
