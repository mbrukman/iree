# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

iree_cc_library(
  NAME
    async_command_queue
  HDRS
    "async_command_queue.h"
  SRCS
    "async_command_queue.cc"
  DEPS
    iree::hal::host::host_submission_queue
    iree::base::status
    iree::base::tracing
    iree::hal::command_queue
    iree::hal::fence
    absl::core_headers
    absl::synchronization
  PUBLIC
)

iree_cc_test(
  NAME
    async_command_queue_test
  SRCS
    "async_command_queue_test.cc"
  DEPS
    iree::hal::host::async_command_queue
    iree::hal::host::host_submission_queue
    iree::base::status
    iree::base::status_matchers
    iree::base::time
    iree::hal::command_queue
    iree::hal::testing::mock_command_buffer
    iree::hal::testing::mock_command_queue
    iree::testing::gtest_main
    absl::memory
    absl::time
)

iree_cc_library(
  NAME
    host_buffer
  HDRS
    "host_buffer.h"
  SRCS
    "host_buffer.cc"
  DEPS
    iree::base::logging
    iree::base::source_location
    iree::base::status
    iree::hal::buffer
    absl::core_headers
  PUBLIC
)

iree_cc_library(
  NAME
    host_event
  HDRS
    "host_event.h"
  SRCS
    "host_event.cc"
  DEPS
    iree::hal::event
  PUBLIC
)

iree_cc_library(
  NAME
    host_fence
  HDRS
    "host_fence.h"
  SRCS
    "host_fence.cc"
  DEPS
    iree::base::status
    iree::base::tracing
    iree::hal::fence
    absl::core_headers
    absl::inlined_vector
    absl::synchronization
    absl::span
  PUBLIC
)

iree_cc_test(
  NAME
    host_fence_test
  SRCS
    "host_fence_test.cc"
  DEPS
    iree::hal::host::host_fence
    iree::base::status
    iree::base::status_matchers
    iree::testing::gtest_main
    absl::time
)

iree_cc_library(
  NAME
    host_local_allocator
  HDRS
    "host_local_allocator.h"
  SRCS
    "host_local_allocator.cc"
  DEPS
    iree::hal::host::host_buffer
    iree::base::source_location
    iree::base::status
    iree::base::tracing
    iree::hal::allocator
    iree::hal::buffer
  PUBLIC
)

iree_cc_library(
  NAME
    host_local_command_processor
  HDRS
    "host_local_command_processor.h"
  SRCS
    "host_local_command_processor.cc"
  DEPS
    iree::base::source_location
    iree::base::status
    iree::base::tracing
    iree::hal::command_buffer
  PUBLIC
)

iree_cc_library(
  NAME
    host_submission_queue
  HDRS
    "host_submission_queue.h"
  SRCS
    "host_submission_queue.cc"
  DEPS
    iree::hal::host::host_fence
    iree::base::intrusive_list
    iree::base::status
    iree::base::tracing
    iree::hal::command_queue
    iree::hal::fence
    iree::hal::semaphore
    absl::core_headers
    absl::inlined_vector
    absl::synchronization
  PUBLIC
)

iree_cc_test(
  NAME
    host_submission_queue_test
  SRCS
    "host_submission_queue_test.cc"
  DEPS
    iree::hal::host::host_submission_queue
    iree::testing::gtest_main
)

iree_cc_library(
  NAME
    inproc_command_buffer
  HDRS
    "inproc_command_buffer.h"
  SRCS
    "inproc_command_buffer.cc"
  DEPS
    iree::base::arena
    iree::base::intrusive_list
    iree::base::status
    iree::base::tracing
    iree::hal::command_buffer
  PUBLIC
)
