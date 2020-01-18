// IREE Hardware Abstraction Layer (HAL) runtime module imports.
//
// This is embedded in the compiler binary and inserted into any module
// containing HAL dialect ops (hal.*) that is lowered to the VM dialect.
vm.module @hal {

//===----------------------------------------------------------------------===//
// Experimental/temporary ops
//===----------------------------------------------------------------------===//

vm.import @ex.shared_device() -> !iree.ref<!hal.device>
attributes {nosideeffects}

// Returns one of the provided executable formats that can be used by the
// device or 0 if none are supported.
vm.import @ex.match_supported_executable_format(
  %device : !iree.ref<!hal.device>,
  %available_formats : i32 ...
) -> i32
attributes {nosideeffects}

// Caches an executable for use with the specified device.
// The executable may be shared with other contexts but as it is immutable
// this does not matter.
vm.import @ex.cache_executable(
  %device : !iree.ref<!hal.device>,
  %executable_format : i32,
  %executable_data : !iree.byte_buffer_ref
) -> !iree.ref<!hal.executable>
attributes {nosideeffects}

vm.import @ex.push_binding(
  %command_buffer : !iree.ref<!hal.command_buffer>,
  %ordinal : i32,
  %buffer : !iree.ref<!hal.buffer>,
  %shape : i32 ...,
  %element_size : i32
)

vm.import @ex.executable_descriptor_set_layout(
  %executable : !iree.ref<!hal.executable>,
  %set : i32
) -> !iree.ref<!hal.descriptor_set_layout>

vm.import @ex.defer_release(
  %operand : !iree.opaque_ref
)

vm.import @ex.submit_and_wait(
  %device : !iree.ref<!hal.device>,
  %command_buffer : !iree.ref<!hal.command_buffer>
)

//===----------------------------------------------------------------------===//
// iree::hal::Allocator
//===----------------------------------------------------------------------===//

// Computes the byte size required for a buffer of the given shape and type.
vm.import @allocator.compute_size(
  %allocator : !iree.ref<!hal.allocator>,
  %memory_types : i32,
  %buffer_usage : i32,
  %shape : i32 ...,
  %element_size : i32
) -> i32
attributes {nosideeffects}

// Allocates a buffer from the allocator.
vm.import @allocator.allocate(
  %allocator : !iree.ref<!hal.allocator>,
  %memory_types : i32,
  %buffer_usage : i32,
  %allocation_size : i32
) -> !iree.ref<!hal.buffer>

// Allocates a buffer from the allocator with the given constant contents.
vm.import @allocator.allocate.const(
  %allocator : !iree.ref<!hal.allocator>,
  %memory_types : i32,
  %buffer_usage : i32,
  %shape : i32 ...,
  %element_size : i32,
  %value : !iree.byte_buffer_ref
) -> !iree.ref<!hal.buffer>

// Allocates a buffer from the allocator.
vm.import @allocator.allocate.shaped(
  %allocator : !iree.ref<!hal.allocator>,
  %memory_types : i32,
  %buffer_usage : i32,
  %shape : i32 ...,
  %element_size : i32
) -> !iree.ref<!hal.buffer>

//===----------------------------------------------------------------------===//
// iree::hal::Buffer
//===----------------------------------------------------------------------===//

// Returns a reference to a subspan of the buffer.
vm.import @buffer.subspan(
  %source_buffer : !iree.ref<!hal.buffer>,
  %source_offset : i32,
  %length : i32
) -> !iree.ref<!hal.buffer>

// Fills the target buffer with the given repeating value.
vm.import @buffer.fill(
  %target_buffer : !iree.ref<!hal.buffer>,
  %target_offset : i32,
  %length : i32,
  %pattern : i32
)

// Reads a block of byte data from the resource at the given offset.
vm.import @buffer.read_data(
  %source_buffer : !iree.ref<!hal.buffer>,
  %source_offset : i32,
  %target_buffer : !iree.mutable_byte_buffer_ref,
  %target_offset : i32,
  %length : i32
)

// Writes a block of byte data into the resource at the given offset.
vm.import @buffer.write_data(
  %target_buffer : !iree.ref<!hal.buffer>,
  %target_offset : i32,
  %source_buffer : !iree.byte_buffer_ref,
  %source_offset : i32,
  %length : i32
)

// Copies data from the provided source_buffer into the buffer.
vm.import @buffer.copy_data(
  %source_buffer : !iree.ref<!hal.buffer>,
  %source_offset : i32,
  %target_buffer : !iree.ref<!hal.buffer>,
  %target_offset : i32,
  %length : i32
)

// Loads a value from a buffer by mapping it.
vm.import @buffer.load(
  %source_buffer : !iree.ref<!hal.buffer>,
  %source_offset : i32,
  %length : i32
) -> i32

// Stores a value into a buffer by mapping it.
vm.import @buffer.store(
  %value : i32,
  %target_buffer : !iree.ref<!hal.buffer>,
  %target_offset : i32,
  %length : i32
)

//===----------------------------------------------------------------------===//
// iree::hal::BufferView
//===----------------------------------------------------------------------===//

// Computes an element byte offset within a buffer.
vm.import @buffer_view.compute_offset(
  %buffer : !iree.ref<!hal.buffer>,
  %shape : i32 ...,
  %indices : i32 ...,
  %element_size : i32
) -> i32

// Computes a shaped buffer view length in bytes.
vm.import @buffer_view.compute_length(
  %buffer : !iree.ref<!hal.buffer>,
  %shape : i32 ...,
  %element_size : i32
) -> i32

// Computes a byte range within a buffer for one or more elements.
vm.import @buffer_view.compute_range(
  %buffer : !iree.ref<!hal.buffer>,
  %shape : i32 ...,
  %indices : i32 ...,
  %lengths : i32 ...,
  %element_size : i32
) -> (i32, i32)

// Returns a view into a buffer. The buffer is not copied and both the original
// and sliced references must be synchronized.
vm.import @buffer_view.slice(
  %buffer : !iree.ref<!hal.buffer>,
  %shape : i32 ...,
  %indices : i32 ...,
  %lengths : i32 ...,
  %element_size : i32
) -> !iree.ref<!hal.buffer>

//===----------------------------------------------------------------------===//
// iree::hal::CommandBuffer
//===----------------------------------------------------------------------===//

// Returns a command buffer from the device pool ready to begin recording.
vm.import @command_buffer.create(
  %device : !iree.ref<!hal.device>,
  %modes : i32,
  %command_categories : i32
) -> !iree.ref<!hal.command_buffer>

// Resets and begins recording into the command buffer, clearing all previously
// recorded contents.
vm.import @command_buffer.begin(
  %command_buffer : !iree.ref<!hal.command_buffer>
)

// Ends recording into the command buffer.
vm.import @command_buffer.end(
  %command_buffer : !iree.ref<!hal.command_buffer>
)

// Defines a memory dependency between commands recorded before and after the
// barrier.
vm.import @command_buffer.execution_barrier(
  %command_buffer : !iree.ref<!hal.command_buffer>,
  %source_stage_mask : i32,
  %target_stage_mask : i32,
  // TODO(benvanik): tuple types.
  %memory_barriers : i32 ...,
  %buffer_barriers : i32 ...
)

// Fills the target buffer with the given repeating value.
vm.import @command_buffer.fill_buffer(
  %command_buffer : !iree.ref<!hal.command_buffer>,
  %target_buffer : !iree.ref<!hal.buffer>,
  %target_offset : i32,
  %length : i32,
  %pattern : i32
)

// Copies a range of one buffer to another.
vm.import @command_buffer.copy_buffer(
  %command_buffer : !iree.ref<!hal.command_buffer>,
  %source_buffer : !iree.ref<!hal.buffer>,
  %source_offset : i32,
  %target_buffer : !iree.ref<!hal.buffer>,
  %target_offset : i32,
  %length : i32
)

// Binds a descriptor set to the given set number.
vm.import @command_buffer.bind_descriptor_set(
  %command_buffer : !iree.ref<!hal.command_buffer>,
  %executable : !iree.ref<!hal.executable>,
  %set : i32,
  %descriptor_set : !iree.ref<!hal.descriptor_set>,
  %dynamic_offsets : i32 ...
)

// Dispatches an execution request.
vm.import @command_buffer.dispatch(
  %command_buffer : !iree.ref<!hal.command_buffer>,
  %executable : !iree.ref<!hal.executable>,
  %entry_point : i32,
  %workgroup_x : i32,
  %workgroup_y : i32,
  %workgroup_z : i32
)

// Dispatches an execution request with the dispatch parameters loaded from the
// given buffer.
vm.import @command_buffer.dispatch.indirect(
  %command_buffer : !iree.ref<!hal.command_buffer>,
  %executable : !iree.ref<!hal.executable>,
  %entry_point : i32,
  %workgroups_buffer : !iree.ref<!hal.buffer>,
  %workgroups_offset : i32
)

//===----------------------------------------------------------------------===//
// iree::hal::DescriptorSet
//===----------------------------------------------------------------------===//

// Allocates a new descriptor set based on the given layout.
vm.import @descriptor_set.allocate(
  %device : !iree.ref<!hal.device>,
  %set_layout : !iree.ref<!hal.descriptor_set_layout>
) -> !iree.ref<!hal.descriptor_set>

// Updates a single binding within a descriptor set.
vm.import @descriptor_set.update(
  %device : !iree.ref<!hal.device>,
  %set : !iree.ref<!hal.descriptor_set>,
  // TODO(benvanik): tuples so that we can batch these updates.
  %binding : i32,
  %buffer : !iree.ref<!hal.buffer>,
  %offset : i32,
  %length : i32,
  %access : i32
)

//===----------------------------------------------------------------------===//
// iree::hal::Device
//===----------------------------------------------------------------------===//

// Returns the allocator that can be used to allocate buffers compatible with
// the device.
vm.import @device.allocator(
  %device : !iree.ref<!hal.device>
) -> !iree.ref<!hal.allocator>
attributes {nosideeffects}

//===----------------------------------------------------------------------===//
// iree::hal::Semaphore
//===----------------------------------------------------------------------===//

// Returns a semaphore from the device pool with the given initial value.
vm.import @semaphore.create(
  %device : !iree.ref<!hal.device>,
  %initial_value : i32
) -> !iree.ref<!hal.semaphore>
attributes {nosideeffects}

// Returns non-zero if the semaphore indicates a failure.
vm.import @semaphore.status(
  %semaphore : !iree.ref<!hal.semaphore>
) -> i32
attributes {nosideeffects}

// Queries the current payload and returns a tuple of `(status, value)`.
// As the payload is monotonically increasing it is guaranteed that
// the value is at least equal to the previous result of a
// `hal.semaphore.signal` call and coherent with any waits for a
// specified value via `hal.semaphore.await`.
vm.import @semaphore.query_value(
  %semaphore : !iree.ref<!hal.semaphore>
) -> (i32, i32)

// Signals the semaphore to the given payload value.
// The call is ignored if the current payload value exceeds |new_value|.
vm.import @semaphore.signal(
  %semaphore : !iree.ref<!hal.semaphore>,
  %new_value : i32
)

// Signals the semaphore with a failure. The |status| will be returned from
// `hal.semaphore.query_value` and `hal.semaphore.signal` for the lifetime
// of the semaphore.
vm.import @semaphore.fail(
  %semaphore : !iree.ref<!hal.semaphore>,
  %status : i32
)

// Yields the caller until the semaphore reaches or exceeds the specified
// payload |value|.
//
// Returns the status of the semaphore after the wait, with a non-zero value
// indicating failure.
vm.import @semaphore.await(
  %semaphore : !iree.ref<!hal.semaphore>,
  %value : i32
) -> i32
// TODO(benvanik): yield point trait.

}  // module
