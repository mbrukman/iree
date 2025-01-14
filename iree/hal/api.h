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

// See iree/base/api.h for documentation on the API conventions used.

#ifndef IREE_HAL_API_H_
#define IREE_HAL_API_H_

#include <stdbool.h>
#include <stdint.h>

#include "iree/base/api.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

//===----------------------------------------------------------------------===//
// Types and Enums
//===----------------------------------------------------------------------===//

typedef struct iree_hal_allocator iree_hal_allocator_t;
typedef struct iree_hal_buffer iree_hal_buffer_t;
typedef struct iree_hal_buffer_view iree_hal_buffer_view_t;
typedef struct iree_hal_command_buffer iree_hal_command_buffer_t;
typedef struct iree_hal_descriptor_set iree_hal_descriptor_set_t;
typedef struct iree_hal_descriptor_set_layout iree_hal_descriptor_set_layout_t;
typedef struct iree_hal_device iree_hal_device_t;
typedef struct iree_hal_driver iree_hal_driver_t;
typedef struct iree_hal_executable iree_hal_executable_t;
typedef struct iree_hal_fence iree_hal_fence_t;
typedef struct iree_hal_semaphore iree_hal_semaphore_t;

// Reference to a buffer's mapped memory.
typedef struct {
  // Contents of the buffer. Behavior is undefined if an access is performed
  // whose type was not specified during mapping.
  iree_byte_span_t contents;

  // Used internally - do not modify.
  uint64_t reserved[8];
} iree_hal_mapped_memory_t;

// A bitfield specifying properties for a memory type.
typedef enum {
  IREE_HAL_MEMORY_TYPE_NONE = 0,

  // Memory is lazily allocated by the device and only exists transiently.
  // This is the optimal mode for memory used only within a single command
  // buffer. Transient buffers, even if they have
  // IREE_HAL_MEMORY_TYPE_HOST_VISIBLE set, should be treated as device-local
  // and opaque as they may have no memory attached to them outside of the time
  // they are being evaluated on devices.
  //
  // This flag can be treated as a hint in most cases; allocating a buffer with
  // it set _may_ return the same as if it had not be set. Certain allocation
  // routines may use the hint to more tightly control reuse or defer wiring the
  // memory.
  IREE_HAL_MEMORY_TYPE_TRANSIENT = 1 << 0,

  // Memory allocated with this type can be mapped for host access using
  // iree_hal_buffer_map.
  IREE_HAL_MEMORY_TYPE_HOST_VISIBLE = 1 << 1,

  // The host cache management commands MappedMemory::Flush and
  // MappedMemory::Invalidate are not needed to flush host writes
  // to the device or make device writes visible to the host, respectively.
  IREE_HAL_MEMORY_TYPE_HOST_COHERENT = 1 << 2,

  // Memory allocated with this type is cached on the host. Host memory
  // accesses to uncached memory are slower than to cached memory, however
  // uncached memory is always host coherent. MappedMemory::Flush must be used
  // to ensure the device has visibility into any changes made on the host and
  // Invalidate must be used to ensure the host has visibility into any changes
  // made on the device.
  IREE_HAL_MEMORY_TYPE_HOST_CACHED = 1 << 3,

  // Memory is accessible as normal host allocated memory.
  IREE_HAL_MEMORY_TYPE_HOST_LOCAL =
      IREE_HAL_MEMORY_TYPE_HOST_VISIBLE | IREE_HAL_MEMORY_TYPE_HOST_COHERENT,

  // Memory allocated with this type is visible to the device for execution.
  // Being device visible does not mean the same thing as
  // IREE_HAL_MEMORY_TYPE_DEVICE_LOCAL. Though an allocation may be visible to
  // the device and therefore useable for execution it may require expensive
  // mapping or implicit transfers.
  IREE_HAL_MEMORY_TYPE_DEVICE_VISIBLE = 1 << 4,

  // Memory allocated with this type is the most efficient for device access.
  // Devices may support using memory that is not device local via
  // IREE_HAL_MEMORY_TYPE_DEVICE_VISIBLE but doing so can incur non-trivial
  // performance penalties. Device local memory, on the other hand, is
  // guaranteed to be fast for all operations.
  IREE_HAL_MEMORY_TYPE_DEVICE_LOCAL =
      IREE_HAL_MEMORY_TYPE_DEVICE_VISIBLE | (1 << 5),
} iree_hal_memory_type_t;

// A bitfield specifying how memory will be accessed in a mapped memory region.
typedef enum {
  // Memory is not mapped.
  IREE_HAL_MEMORY_ACCESS_NONE = 0,
  // Memory will be read.
  // If a buffer is only mapped for reading it may still be possible to write to
  // it but the results will be undefined (as it may present coherency issues).
  IREE_HAL_MEMORY_ACCESS_READ = 1 << 0,
  // Memory will be written.
  // If a buffer is only mapped for writing it may still be possible to read
  // from it but the results will be undefined or incredibly slow (as it may
  // be mapped by the driver as uncached).
  IREE_HAL_MEMORY_ACCESS_WRITE = 1 << 1,
  // Memory will be discarded prior to mapping.
  // The existing contents will be undefined after mapping and must be written
  // to ensure validity.
  IREE_HAL_MEMORY_ACCESS_DISCARD = 1 << 2,
  // Memory will be discarded and completely overwritten in a single operation.
  IREE_HAL_MEMORY_ACCESS_DISCARD_WRITE =
      IREE_HAL_MEMORY_ACCESS_WRITE | IREE_HAL_MEMORY_ACCESS_DISCARD,
  // Memory may have any operation performed on it.
  IREE_HAL_MEMORY_ACCESS_ALL = IREE_HAL_MEMORY_ACCESS_READ |
                               IREE_HAL_MEMORY_ACCESS_WRITE |
                               IREE_HAL_MEMORY_ACCESS_DISCARD,
} iree_hal_memory_access_t;

// Bitfield that defines how a buffer is intended to be used.
// Usage allows the driver to appropriately place the buffer for more
// efficient operations of the specified types.
typedef enum {
  IREE_HAL_BUFFER_USAGE_NONE = 0,

  // The buffer, once defined, will not be mapped or updated again.
  // This should be used for uniform parameter values such as runtime
  // constants for executables. Doing so may allow drivers to inline values or
  // represent them in command buffers more efficiently (avoiding memory reads
  // or swapping, etc).
  IREE_HAL_BUFFER_USAGE_CONSTANT = 1 << 0,

  // The buffer can be used as the source or target of a transfer command
  // (CopyBuffer, UpdateBuffer, etc).
  //
  // If |IREE_HAL_BUFFER_USAGE_MAPPING| is not specified drivers may safely
  // assume that the host may never need visibility of this buffer as all
  // accesses will happen via command buffers.
  IREE_HAL_BUFFER_USAGE_TRANSFER = 1 << 1,

  // The buffer can be mapped by the host application for reading and writing.
  //
  // As mapping may require placement in special address ranges or system
  // calls to enable visibility the driver can use the presence (or lack of)
  // this flag to perform allocation-type setup and avoid initial mapping
  // overhead.
  IREE_HAL_BUFFER_USAGE_MAPPING = 1 << 2,

  // The buffer can be provided as an input or output to an executable.
  // Buffers of this type may be directly used by drivers during dispatch.
  IREE_HAL_BUFFER_USAGE_DISPATCH = 1 << 3,

  // Buffer may be used for any operation.
  IREE_HAL_BUFFER_USAGE_ALL = IREE_HAL_BUFFER_USAGE_TRANSFER |
                              IREE_HAL_BUFFER_USAGE_MAPPING |
                              IREE_HAL_BUFFER_USAGE_DISPATCH,
} iree_hal_buffer_usage_t;

// An opaque driver-specific handle to identify different devices.
typedef uintptr_t iree_hal_device_id_t;

// Describes an enumerated HAL device.
typedef struct {
  // Opaque handle used by drivers. Not valid across driver instances.
  iree_hal_device_id_t device_id;
  // Name of the device as returned by the API.
  iree_string_view_t name;
} iree_hal_device_info_t;

// A bitfield specifying the mode of operation for a command buffer.
typedef enum {
  // Command buffer will be submitted once and never used again.
  // This may enable in-place patching of command buffers that reduce overhead
  // when it's known that command buffers will not be reused.
  IREE_HAL_COMMAND_BUFFER_MODE_ONE_SHOT = 1 << 0,
} iree_hal_command_buffer_mode_t;

// A bitfield specifying the category of commands in a command queue.
typedef enum {
  // Command is considered a transfer operation (memcpy, etc).
  IREE_HAL_COMMAND_CATEGORY_TRANSFER = 1 << 0,
  // Command is considered a dispatch operation (dispatch/execute).
  IREE_HAL_COMMAND_CATEGORY_DISPATCH = 1 << 1,
} iree_hal_command_category_t;

// Bitfield specifying which execution stage a brarrier should start/end at.
//
// Maps to VkPipelineStageFlagBits.
typedef enum {
  // Top of the pipeline when commands are initially issued by the device.
  IREE_HAL_EXECUTION_STAGE_COMMAND_ISSUE = 1 << 0,
  // Stage of the pipeline when dispatch parameter data is consumed.
  IREE_HAL_EXECUTION_STAGE_COMMAND_PROCESS = 1 << 1,
  // Stage where dispatch commands execute.
  IREE_HAL_EXECUTION_STAGE_DISPATCH = 1 << 2,
  // Stage where transfer (copy/clear/fill/etc) commands execute.
  IREE_HAL_EXECUTION_STAGE_TRANSFER = 1 << 3,
  // Final stage in the pipeline when commands are retired on the device.
  IREE_HAL_EXECUTION_STAGE_COMMAND_RETIRE = 1 << 4,
  // Pseudo-stage for read/writes by the host. Not executed on device.
  IREE_HAL_EXECUTION_STAGE_HOST = 1 << 5,
} iree_hal_execution_stage_t;

// Bitfield specifying which scopes will access memory and how.
//
// Maps to VkAccessFlagBits.
typedef enum {
  // Read access to indirect command data as part of an indirect dispatch.
  IREE_HAL_ACCESS_SCOPE_INDIRECT_COMMAND_READ = 1 << 0,
  // Constant uniform buffer reads by the device.
  IREE_HAL_ACCESS_SCOPE_CONSTANT_READ = 1 << 1,
  // Storage buffer reads by dispatch commands.
  IREE_HAL_ACCESS_SCOPE_DISPATCH_READ = 1 << 2,
  // Storage buffer writes by dispatch commands.
  IREE_HAL_ACCESS_SCOPE_DISPATCH_WRITE = 1 << 3,
  // Source of a transfer operation.
  IREE_HAL_ACCESS_SCOPE_TRANSFER_READ = 1 << 4,
  // Target of a transfer operation.
  IREE_HAL_ACCESS_SCOPE_TRANSFER_WRITE = 1 << 5,
  // Read operation by the host through mapped memory.
  IREE_HAL_ACCESS_SCOPE_HOST_READ = 1 << 6,
  // Write operation by the host through mapped memory.
  IREE_HAL_ACCESS_SCOPE_HOST_WRITE = 1 << 7,
  // External/non-specific read.
  IREE_HAL_ACCESS_SCOPE_MEMORY_READ = 1 << 8,
  // External/non-specific write.
  IREE_HAL_ACCESS_SCOPE_MEMORY_WRITE = 1 << 9,
} iree_hal_access_scope_t;

// Defines a global memory barrier.
// These are cheaper to encode than buffer-specific barriers but may cause
// stalls and bubbles in device pipelines if applied too broadly. Prefer them
// over equivalently large sets of buffer-specific barriers (such as when
// completely changing execution contexts).
//
// Maps to VkMemoryBarrier.
typedef struct {
  // All access scopes prior-to the barrier (inclusive).
  iree_hal_access_scope_t source_scope;
  // All access scopes following the barrier (inclusive).
  iree_hal_access_scope_t target_scope;
} iree_hal_memory_barrier_t;

// Defines a memory barrier that applies to a range of a specific buffer.
// Use of these (vs. global memory barriers) provides fine-grained execution
// ordering to device command processors and allows for more aggressive
// reordering.
//
// Maps to VkBufferMemoryBarrier.
typedef struct {
  // All access scopes prior-to the barrier (inclusive).
  iree_hal_access_scope_t source_scope;
  // All access scopes following the barrier (inclusive).
  iree_hal_access_scope_t target_scope;
  // Buffer the barrier is restricted to.
  // The barrier will apply to the entire physical device allocation.
  iree_hal_buffer_t* buffer;
  // Relative offset/length within |buffer| (which may itself be mapped into the
  // device allocation at an offset).
  iree_device_size_t offset;
  iree_device_size_t length;
} iree_hal_buffer_barrier_t;

//===----------------------------------------------------------------------===//
// iree::hal::Allocator
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Retains the given |allocator| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_allocator_retain(iree_hal_allocator_t* allocator);

// Releases the given |allocator| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_allocator_release(iree_hal_allocator_t* allocator);

// Allocates a buffer from the allocator.
// Fails if the memory type requested for the given usage cannot be serviced.
// Callers can use iree_hal_allocator_can_allocate to decide their memory use
// strategy.
//
// The memory type of the buffer returned may differ from the requested value
// if the device can provide more functionality; for example, if requesting
// MemoryType::kHostVisible but the memory is really host cached you may get
// a buffer back with MemoryType::kHostVisible | MemoryType::kHostCached. The
// only requirement is that the buffer satisfy the required bits.
//
// Fails if it is not possible to allocate and satisfy all placements for the
// requested |buffer_usage|.
// |out_buffer| must be released by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_allocator_allocate_buffer(
    iree_hal_allocator_t* allocator, iree_hal_memory_type_t memory_type,
    iree_hal_buffer_usage_t buffer_usage, iree_host_size_t allocation_size,
    iree_hal_buffer_t** out_buffer);

// Wraps an existing host allocation in a buffer.
// Ownership of the allocation remains with the caller and the memory must
// remain valid for so long as the buffer may be in use.
//
// Fails if the allocator cannot access host memory in this way.
// |out_buffer| must be released by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_allocator_wrap_buffer(
    iree_hal_allocator_t* allocator, iree_hal_memory_type_t memory_type,
    iree_hal_memory_access_t allowed_access,
    iree_hal_buffer_usage_t buffer_usage, iree_byte_span_t data,
    iree_hal_buffer_t** out_buffer);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::Buffer
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Returns a reference to a subspan of the |buffer|.
// If |byte_length| is IREE_WHOLE_BUFFER the remaining bytes in the buffer after
// |byte_offset| (possibly 0) will be selected.
//
// The parent buffer will remain alive for the lifetime of the subspan
// returned. If the subspan is a small portion this may cause additional
// memory to remain allocated longer than required.
//
// Returns the given |buffer| if the requested span covers the entire range.
// |out_buffer| must be released by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_subspan(
    iree_hal_buffer_t* buffer, iree_device_size_t byte_offset,
    iree_device_size_t byte_length, iree_allocator_t allocator,
    iree_hal_buffer_t** out_buffer);

// Retains the given |buffer| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_buffer_retain(iree_hal_buffer_t* buffer);

// Releases the given |buffer| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_buffer_release(iree_hal_buffer_t* buffer);

// Returns the size in bytes of the buffer.
IREE_API_EXPORT iree_device_size_t IREE_API_CALL
iree_hal_buffer_byte_length(const iree_hal_buffer_t* buffer);

// Sets a range of the buffer to binary zero.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_buffer_zero(iree_hal_buffer_t* buffer, iree_device_size_t byte_offset,
                     iree_device_size_t byte_length);

// Sets a range of the buffer to the given value.
// Only |pattern_length| values with 1, 2, or 4 bytes are supported.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_buffer_fill(iree_hal_buffer_t* buffer, iree_device_size_t byte_offset,
                     iree_device_size_t byte_length, const void* pattern,
                     iree_host_size_t pattern_length);

// Reads a block of data from the buffer at the given offset.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_read_data(
    iree_hal_buffer_t* buffer, iree_device_size_t source_offset,
    void* target_buffer, iree_device_size_t data_length);

// Writes a block of byte data into the buffer at the given offset.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_write_data(
    iree_hal_buffer_t* buffer, iree_device_size_t target_offset,
    const void* source_buffer, iree_device_size_t data_length);

// Maps the buffer to be accessed as a host pointer into |out_mapped_memory|.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_map(
    iree_hal_buffer_t* buffer, iree_hal_memory_access_t memory_access,
    iree_device_size_t element_offset, iree_device_size_t element_length,
    iree_hal_mapped_memory_t* out_mapped_memory);

// Unmaps the buffer as was previously mapped to |mapped_memory|.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_unmap(
    iree_hal_buffer_t* buffer, iree_hal_mapped_memory_t* mapped_memory);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::HeapBuffer
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Allocates a zeroed host heap buffer of the given size.
// The buffer contents will be allocated with |contents_allocator| while
// |allocator| is used for the iree_hal_buffer_t.
//
// Returns a buffer allocated with malloc that may not be usable by devices
// without copies. |memory_type| should be set to
// IREE_HAL_MEMORY_TYPE_HOST_LOCAL in most cases.
// |out_buffer| must be released by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_heap_buffer_allocate(
    iree_hal_memory_type_t memory_type, iree_hal_buffer_usage_t usage,
    iree_host_size_t allocation_size, iree_allocator_t contents_allocator,
    iree_allocator_t allocator, iree_hal_buffer_t** out_buffer);

// Allocates a host heap buffer with a copy of the given data.
// The buffer contents will be allocated with |contents_allocator| while
// |allocator| is used for the iree_hal_buffer_t.
//
// Returns a buffer allocated with malloc that may not be usable by devices
// without copies. |memory_type| should be set to
// IREE_HAL_MEMORY_TYPE_HOST_LOCAL in most cases.
// |out_buffer| must be released by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_heap_buffer_allocate_copy(
    iree_hal_memory_type_t memory_type, iree_hal_buffer_usage_t usage,
    iree_hal_memory_access_t allowed_access, iree_byte_span_t contents,
    iree_allocator_t contents_allocator, iree_allocator_t allocator,
    iree_hal_buffer_t** out_buffer);

// Wraps an existing host heap allocation in a buffer.
// Ownership of the host allocation remains with the caller and the memory
// must remain valid for so long as the iree_hal_buffer_t may be in use.
//
// Returns a buffer allocated with malloc that may not be usable by devices
// without copies. |memory_type| should be set to
// IREE_HAL_MEMORY_TYPE_HOST_LOCAL in most cases.
// |out_buffer| must be released by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_heap_buffer_wrap(
    iree_hal_memory_type_t memory_type, iree_hal_memory_access_t allowed_access,
    iree_hal_buffer_usage_t usage, iree_byte_span_t contents,
    iree_allocator_t allocator, iree_hal_buffer_t** out_buffer);

// TODO(benvanik): add a wrap that takes an allocator just for the buffer.

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::BufferView
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Creates a buffer view with the given |buffer|, which may be nullptr.
// |out_buffer_view| must be released by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_view_create(
    iree_hal_buffer_t* buffer, iree_shape_t shape, int8_t element_size,
    iree_allocator_t allocator, iree_hal_buffer_view_t** out_buffer_view);

// Retains the given |buffer_view| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_buffer_view_retain(iree_hal_buffer_view_t* buffer_view);

// Releases the given |buffer_view| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_buffer_view_release(iree_hal_buffer_view_t* buffer_view);

// Sets the buffer view to point at the new |buffer| with the given metadata.
// To clear a buffer_view to empty use iree_hal_buffer_view_reset.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_view_assign(
    iree_hal_buffer_view_t* buffer_view, iree_hal_buffer_t* buffer,
    iree_shape_t shape, int8_t element_size);

// Resets the buffer view to have an empty buffer and shape.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_buffer_view_reset(iree_hal_buffer_view_t* buffer_view);

// Returns the buffer underlying the buffer view.
// The caller must retain the returned buffer if they want to continue using it.
IREE_API_EXPORT iree_hal_buffer_t* IREE_API_CALL
iree_hal_buffer_view_buffer(const iree_hal_buffer_view_t* buffer_view);

// Returns the shape of the buffer view in |out_shape|.
// If there is not enough space in |out_shape| to store all dimensions then
// IREE_STATUS_OUT_OF_RANGE is returned and |out_shape|.rank is set to the rank.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_buffer_view_shape(
    const iree_hal_buffer_view_t* buffer_view, iree_shape_t* out_shape);

// Returns the size of each element in the buffer view in bytes.
IREE_API_EXPORT int8_t IREE_API_CALL
iree_hal_buffer_view_element_size(const iree_hal_buffer_view_t* buffer_view);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::CommandBuffer
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Creates a command buffer ready to begin recording, possibly reusing an
// existing one from the |device| pool.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_command_buffer_create(
    iree_hal_device_t* device, iree_hal_command_buffer_mode_t mode,
    iree_hal_command_category_t command_categories, iree_allocator_t allocator,
    iree_hal_command_buffer_t** out_command_buffer);

// Retains the given |command_buffer| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_command_buffer_retain(iree_hal_command_buffer_t* command_buffer);

// Releases the given |command_buffer| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_command_buffer_release(iree_hal_command_buffer_t* command_buffer);

// Resets and begins recording into the command buffer, clearing all
// previously recorded contents.
// The command buffer must not be in-flight.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_command_buffer_begin(iree_hal_command_buffer_t* command_buffer);

// Ends recording into the command buffer.
// This must be called prior to submitting the command buffer for execution.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_command_buffer_end(iree_hal_command_buffer_t* command_buffer);

// Defines a memory dependency between commands recorded before and after the
// barrier. One or more memory or buffer barriers can be specified to indicate
// between which stages or buffers the dependencies exist.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_command_buffer_execution_barrier(
    iree_hal_command_buffer_t* command_buffer,
    iree_hal_execution_stage_t source_stage_mask,
    iree_hal_execution_stage_t target_stage_mask,
    iree_host_size_t memory_barrier_count,
    const iree_hal_memory_barrier_t* memory_barriers,
    iree_host_size_t buffer_barrier_count,
    const iree_hal_buffer_barrier_t* buffer_barriers);

// Copies a range of one buffer to another.
// Both buffers must be compatible with the devices owned by this device
// queue and be allocated with IREE_HAL_BUFFER_USAGE_TRANSFER. Though the source
// and target buffer may be the same the ranges must not overlap (as with
// memcpy).
//
// This can be used to perform device->host, host->device, and device->device
// copies.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_command_buffer_copy_buffer(
    iree_hal_command_buffer_t* command_buffer, iree_hal_buffer_t* source_buffer,
    iree_device_size_t source_offset, iree_hal_buffer_t* target_buffer,
    iree_device_size_t target_offset, iree_device_size_t length);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::Device
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Retains the given |device| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_device_retain(iree_hal_device_t* device);

// Releases the given |device| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_device_release(iree_hal_device_t* device);

// Returns a reference to the allocator of the device that can be used for
// allocating buffers.
IREE_API_EXPORT iree_hal_allocator_t* IREE_API_CALL
iree_hal_device_allocator(iree_hal_device_t* device);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::Driver
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Retains the given |driver| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_retain(iree_hal_driver_t* driver);

// Releases the given |driver| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_release(iree_hal_driver_t* driver);

// Queries available devices and returns them as a list.
// The provided |allocator| will be used to allocate the returned list and after
// the caller is done with it |out_device_infos| must be freed with that same
// allocator by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_query_available_devices(
    iree_hal_driver_t* driver, iree_allocator_t allocator,
    iree_hal_device_info_t** out_device_infos,
    iree_host_size_t* out_device_info_count);

// Creates a device as queried with iree_hal_driver_query_available_devices.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_driver_create_device(
    iree_hal_driver_t* driver, iree_hal_device_id_t device_id,
    iree_allocator_t allocator, iree_hal_device_t** out_device);

// Creates the driver-defined "default" device. This may simply be the first
// device enumerated.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_create_default_device(iree_hal_driver_t* driver,
                                      iree_allocator_t allocator,
                                      iree_hal_device_t** out_device);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::DriverRegistry
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Returns true if the given |driver_name| is registered.
// If this returns false when unexpected ensure that the driver is linked into
// the binary.
IREE_API_EXPORT bool IREE_API_CALL
iree_hal_driver_registry_has_driver(iree_string_view_t driver_name);

// Queries registered drivers and returns them as a list.
// The provided |allocator| will be used to allocate the returned list and after
// the caller is done with it |out_driver_names| must be freed with that same
// allocator by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_registry_query_available_drivers(
    iree_allocator_t allocator, iree_string_view_t** out_driver_names,
    iree_host_size_t* out_driver_count);

// Creates a driver registered with the driver registry by name.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_registry_create_driver(iree_string_view_t driver_name,
                                       iree_allocator_t allocator,
                                       iree_hal_driver_t** out_driver);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::Executable
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Retains the given |executable| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_executable_retain(iree_hal_executable_t* executable);

// Releases the given |executable| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_executable_release(iree_hal_executable_t* executable);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::Fence
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Retains the given |fence| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_fence_retain(iree_hal_fence_t* fence);

// Releases the given |fence| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_fence_release(iree_hal_fence_t* fence);

#endif  // IREE_API_NO_PROTOTYPES

//===----------------------------------------------------------------------===//
// iree::hal::Semaphore
//===----------------------------------------------------------------------===//

#ifndef IREE_API_NO_PROTOTYPES

// Retains the given |semaphore| for the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_semaphore_retain(iree_hal_semaphore_t* semaphore);

// Releases the given |semaphore| from the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_semaphore_release(iree_hal_semaphore_t* semaphore);

#endif  // IREE_API_NO_PROTOTYPES

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_HAL_API_H_
