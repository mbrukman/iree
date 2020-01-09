// RUN: iree-opt -split-input-file -iree-index-computation -simplify-spirv-affine-exprs=false -convert-iree-to-spirv -verify-diagnostics -o - %s | IreeFileCheck %s

#map0 = (d0, d1, d2) -> (d1, d0)
#map1 = (d0, d1, d2) -> (d2, d1, d0)

module {
  // CHECK:spv.module "Logical" "GLSL450"
  // CHECK-DAG: spv.globalVariable [[GLOBALIDVAR:@.*]] built_in("GlobalInvocationId") : !spv.ptr<vector<3xi32>, Input>
  // CHECK: func [[FN:@broadcast_2D_3D]]
  // CHECK-SAME: [[ARG0:%[a-zA-Z0-9_]*]]: !spv.ptr<!spv.struct<!spv.array<504 x i32 [4]> [0]>, StorageBuffer>
  // CHECK-SAME: [[ARG1:%[a-zA-Z0-9_]*]]: !spv.ptr<!spv.struct<!spv.array<1512 x i32 [4]> [0]>, StorageBuffer>
  func @broadcast_2D_3D(%arg0: memref<12x42xi32> {iree.index_computation_info = [[#map0]]}, %arg1: memref<3x12x42xi32>) attributes {iree.executable.export, iree.executable.workgroup_size = dense<[32, 1, 1]> : tensor<3xi32>, iree.executable.workload = dense<[42, 12, 3]> : tensor<3xi32>, iree.num_dims = 3 : i32, iree.ordinal = 0 : i32} {
    // CHECK: [[ARG0LOADPTR:%.*]] = spv.AccessChain [[ARG0]]
    // CHECK: [[VAL:%.*]] = spv.Load "StorageBuffer" [[ARG0LOADPTR]]
    %0 = iree.load_input(%arg0 : memref<12x42xi32>)  {iree.index_computation_info = [[#map0, #map0]]} : tensor<12x42xi32>
    // CHECK: [[ARG1STOREPTR:%.*]] = spv.AccessChain [[ARG1]]
    // CHECK: spv.Store "StorageBuffer" [[ARG1STOREPTR]], [[VAL]]
    %1 = "xla_hlo.broadcast"(%0) {broadcast_sizes = dense<3> : tensor<1xi64>, iree.index_computation_info = [[#map1, #map0]]} : (tensor<12x42xi32>) -> tensor<3x12x42xi32>
    iree.store_output(%1 : tensor<3x12x42xi32>, %arg1 : memref<3x12x42xi32>)
    iree.return
  }
}

// -----

#map0 = (d0, d1, d2) -> (0)
#map1 = (d0, d1, d2) -> (d2, d1, d0)

module {
  // CHECK:spv.module "Logical" "GLSL450"
  // CHECK-DAG: spv.globalVariable [[GLOBALIDVAR:@.*]] built_in("GlobalInvocationId") : !spv.ptr<vector<3xi32>, Input>
  // CHECK: func [[FN:@broadcast_scalar_3D]]
  // CHECK-SAME: [[ARG0:%[a-zA-Z0-9_]*]]: !spv.ptr<!spv.struct<i32 [0]>, StorageBuffer>
  // CHECK-SAME: [[ARG1:%[a-zA-Z0-9_]*]]: !spv.ptr<!spv.struct<!spv.array<1512 x i32 [4]> [0]>, StorageBuffer>
  func @broadcast_scalar_3D(%arg0: memref<i32> {iree.index_computation_info = [[#map0]]}, %arg1: memref<3x12x42xi32>) attributes {iree.executable.export, iree.executable.workgroup_size = dense<[32, 1, 1]> : tensor<3xi32>, iree.executable.workload = dense<[42, 12, 3]> : tensor<3xi32>, iree.num_dims = 3 : i32, iree.ordinal = 0 : i32} {
    // CHECK: [[ARG0LOADPTR:%.*]] = spv.AccessChain [[ARG0]]
    // CHECK: [[VAL:%.*]] = spv.Load "StorageBuffer" [[ARG0LOADPTR]]
    %0 = iree.load_input(%arg0 : memref<i32>)  {iree.index_computation_info = [[#map0, #map0]]} : tensor<i32>
    %1 = "xla_hlo.broadcast"(%0) {broadcast_sizes = dense<[3, 12, 42]> : tensor<3xi64>, iree.index_computation_info = [[#map1, #map0]]} : (tensor<i32>) -> tensor<3x12x42xi32>
    // CHECK: [[ARG1STOREPTR:%.*]] = spv.AccessChain [[ARG1]]
    // CHECK: spv.Store "StorageBuffer" [[ARG1STOREPTR]], [[VAL]]
    iree.store_output(%1 : tensor<3x12x42xi32>, %arg1 : memref<3x12x42xi32>)
    iree.return
  }
}