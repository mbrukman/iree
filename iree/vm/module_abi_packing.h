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

#ifndef IREE_VM_MODULE_ABI_PACKING_H_
#define IREE_VM_MODULE_ABI_PACKING_H_

#include <memory>
#include <tuple>
#include <utility>

#include "absl/types/span.h"
#include "iree/base/api.h"
#include "iree/base/api_util.h"
#include "iree/base/status.h"
#include "iree/vm/module.h"
#include "iree/vm/ref.h"
#include "iree/vm/ref_cc.h"
#include "iree/vm/stack.h"
#include "iree/vm/types.h"

namespace iree {
namespace vm {
namespace packing {

//===----------------------------------------------------------------------===//
// Parameter unpacking
//===----------------------------------------------------------------------===//

struct ParamUnpackState {
  int i32_ordinal = 0;
  int ref_ordinal = 0;
  int varargs_ordinal = 0;
  Status status;
};

template <typename T>
struct ParamUnpack {
  ParamUnpack(ParamUnpackState* param_state, iree_vm_stack_frame_t* frame) {
    ++param_state->varargs_ordinal;
    reg = static_cast<T>(frame->registers.i32[param_state->i32_ordinal++]);
  }
  operator T() const { return reg; }
  T reg;
};

template <>
struct ParamUnpack<opaque_ref> {
  ParamUnpack(ParamUnpackState* param_state, iree_vm_stack_frame_t* frame) {
    ++param_state->varargs_ordinal;
    iree_vm_ref_move(&frame->registers.ref[param_state->ref_ordinal++],
                     &reg.value);
  }
  operator opaque_ref&() { return reg; }
  operator const opaque_ref &() const { return reg; }
  opaque_ref reg;
};

template <typename T>
struct ParamUnpack<ref<T>> {
  ParamUnpack(ParamUnpackState* param_state, iree_vm_stack_frame_t* frame) {
    ++param_state->varargs_ordinal;
    auto& ref_storage = frame->registers.ref[param_state->ref_ordinal++];
    if (ref_storage.type == ref_type_descriptor<T>::get()->type) {
      // Move semantics.
      reg = ref<T>{reinterpret_cast<T*>(ref_storage.ptr)};
      std::memset(&ref_storage, 0, sizeof(ref_storage));
    } else if (ref_storage.type != IREE_VM_REF_TYPE_NULL) {
      param_state->status =
          InvalidArgumentErrorBuilder(IREE_LOC)
          << "Parameter contains a reference to the wrong type; have "
          << iree_vm_ref_type_name(ref_storage.type).data << " but expected "
          << ref_type_descriptor<T>::get()->type_name.data << " ("
          << typeid(reg).name() << ")";
    }
    // NOTE: null is allowed here!
  }
  operator ref<T> &() { return reg; }
  operator const ref<T> &() const { return reg; }
  ref<T> reg;
};

template <typename U, size_t S>
struct ParamUnpack<std::array<U, S>>;
template <typename... Ts>
struct ParamUnpack<std::tuple<Ts...>>;
template <typename U>
struct ParamUnpack<absl::Span<U>>;

template <typename U, size_t S>
struct ParamUnpack<std::array<U, S>> {
  ParamUnpack(ParamUnpackState* param_state, iree_vm_stack_frame_t* frame) {
    ++param_state->varargs_ordinal;
    regs = UnpackArray<U>(param_state, frame, std::make_index_sequence<S>());
  }
  template <typename T, size_t... I>
  inline std::array<T, sizeof...(I)> UnpackArray(ParamUnpackState* param_state,
                                                 iree_vm_stack_frame_t* frame,
                                                 std::index_sequence<I...>) {
    return {((void)I, ParamUnpack<T>(param_state, frame))...};
  }
  operator std::array<U, S> &() { return regs; }
  operator std::array<U, S>() const { return regs; }
  std::array<U, S> regs;
};

template <typename... Ts>
struct ParamUnpack<std::tuple<Ts...>> {
  ParamUnpack(ParamUnpackState* param_state, iree_vm_stack_frame_t* frame) {
    ++param_state->varargs_ordinal;
    regs = std::make_tuple(ParamUnpack<Ts>(param_state, frame)...);
  }
  operator std::tuple<Ts...> &() { return regs; }
  operator std::tuple<Ts...>() const { return regs; }
  std::tuple<Ts...> regs;
};

template <typename U>
struct ParamUnpack<absl::Span<U>> {
  ParamUnpack(ParamUnpackState* param_state, iree_vm_stack_frame_t* frame) {
    uint8_t count =
        frame->return_registers->registers[param_state->varargs_ordinal++];
    int32_t original_varargs_ordinal = param_state->varargs_ordinal;
    regs.reserve(count);
    for (int i = 0; i < count; ++i) {
      regs.push_back(
          ParamUnpack<typename std::decay<U>::type>(param_state, frame));
    }
    param_state->varargs_ordinal = original_varargs_ordinal;
  }
  operator absl::Span<U>() const { return absl::MakeSpan(regs); }
  mutable std::vector<typename std::decay<U>::type> regs;
};

//===----------------------------------------------------------------------===//
// Result packing
//===----------------------------------------------------------------------===//

struct ResultPackState {
  int i32_ordinal = 0;
  int ref_ordinal = 0;
  Status status;
};

template <typename T>
struct ResultCount {
  constexpr static int value = 1;
};
template <typename... Ts>
struct ResultCount<std::tuple<Ts...>> {
  template <int I, typename... Tail>
  struct Adder;
  template <int I, typename T, typename... Tail>
  struct Adder<I, T, Tail...> {
    constexpr static int value =
        ResultCount<T>::value + Adder<sizeof...(Tail), Tail...>::value;
  };
  template <typename T, typename... Tail>
  struct Adder<1, T, Tail...> {
    constexpr static int value = ResultCount<T>::value;
  };
  constexpr static int value = Adder<sizeof...(Ts), Ts...>::value;
};

template <typename T>
struct ResultRegister {
  constexpr static auto value = std::make_tuple<uint8_t>(0);
};
template <typename T>
struct ResultRegister<ref<T>> {
  constexpr static auto value = std::make_tuple<uint8_t>(
      IREE_REF_REGISTER_TYPE_BIT | IREE_REF_REGISTER_MOVE_BIT);
};
template <typename... Ts>
struct ResultRegister<std::tuple<Ts...>> {
  constexpr static auto value = std::tuple_cat(ResultRegister<Ts>::value...);
};

template <typename T>
struct ResultPack {
  ResultPack(ResultPackState* result_state, iree_vm_stack_frame_t* frame,
             T value) {
    frame->registers.i32[result_state->i32_ordinal++] =
        static_cast<int32_t>(value);
  }
};
template <typename T>
struct ResultPack<ref<T>> {
  ResultPack(ResultPackState* result_state, iree_vm_stack_frame_t* frame,
             ref<T> value) {
    // TODO(benvanik): only clear the output if we didn't already do it for
    // a parameter read.
    auto* reg_ptr = &frame->registers.ref[result_state->ref_ordinal++];
    std::memset(reg_ptr, 0, sizeof(*reg_ptr));
    iree_vm_ref_wrap_assign(value.release(), value.type(), reg_ptr);
  }
};

template <typename... Ts>
struct ResultPack<std::tuple<Ts...>> {
  ResultPack(ResultPackState* result_state, iree_vm_stack_frame_t* frame,
             std::tuple<Ts...> results) {
    PackTuple(result_state, frame, results,
              std::make_index_sequence<sizeof...(Ts)>());
  }

  template <typename... T, size_t... I>
  inline std::tuple<ResultPack<T>...> PackTuple(ResultPackState* result_state,
                                                iree_vm_stack_frame_t* frame,
                                                std::tuple<T...>& value,
                                                std::index_sequence<I...>) {
    return std::make_tuple<ResultPack<T>...>(
        ResultPack<typename std::tuple_element<I, std::tuple<T...>>::type>(
            result_state, frame, std::move(std::get<I>(value)))...);
  }
};

//===----------------------------------------------------------------------===//
// Function wrapping
//===----------------------------------------------------------------------===//

template <typename T, uint8_t... I>
constexpr auto ConstTupleOr(std::integer_sequence<uint8_t, I...>) {
  return std::make_tuple(
      (static_cast<uint8_t>(std::get<I>(ResultRegister<T>::value) | I))...);
}

template <typename T, size_t... I>
constexpr auto TupleToArray(const T& t, std::index_sequence<I...>) {
  return std::array<uint8_t, std::tuple_size<T>::value>{std::get<I>(t)...};
}

template <typename Owner, typename Results, typename... Params>
struct DispatchFunctor {
  using FnPtr = StatusOr<Results> (Owner::*)(Params...);

  static Status Call(void (Owner::*ptr)(), Owner* self, iree_vm_stack_t* stack,
                     iree_vm_stack_frame_t* frame,
                     iree_vm_execution_result_t* out_result) {
    ParamUnpackState param_state;
    auto params = std::make_tuple(
        ParamUnpack<typename std::decay<Params>::type>(&param_state, frame)...);
    RETURN_IF_ERROR(param_state.status);

    frame->return_registers = nullptr;
    frame->registers.ref_register_count = 0;

    auto results_or =
        ApplyFn(reinterpret_cast<FnPtr>(ptr), self, std::move(params),
                std::make_index_sequence<sizeof...(Params)>());
    if (!results_or.ok()) {
      return std::move(results_or).status();
    }

    static const int kResultCount = ResultCount<Results>::value;
    static const auto kResultList = TupleToArray(
        std::tuple_cat(
            std::make_tuple<uint8_t>(kResultCount),
            ConstTupleOr<Results>(
                std::make_integer_sequence<uint8_t, kResultCount>())),
        std::make_index_sequence<1 + kResultCount>());
    frame->return_registers =
        reinterpret_cast<const iree_vm_register_list_t*>(kResultList.data());

    ResultPackState result_state;
    auto results = std::move(results_or).ValueOrDie();
    ResultPack<Results>(&result_state, frame, std::move(results));
    return result_state.status;
  }

  template <size_t... I>
  static StatusOr<Results> ApplyFn(
      FnPtr ptr, Owner* self,
      std::tuple<ParamUnpack<typename std::decay<Params>::type>...>&& params,
      std::index_sequence<I...>) {
    return (self->*ptr)(std::get<I>(params)...);
  }
};

template <typename Owner, typename... Params>
struct DispatchFunctorVoid {
  using FnPtr = Status (Owner::*)(Params...);

  static Status Call(void (Owner::*ptr)(), Owner* self, iree_vm_stack_t* stack,
                     iree_vm_stack_frame_t* frame,
                     iree_vm_execution_result_t* out_result) {
    ParamUnpackState param_state;
    auto params = std::make_tuple(
        ParamUnpack<typename std::decay<Params>::type>(&param_state, frame)...);
    RETURN_IF_ERROR(param_state.status);

    frame->return_registers = nullptr;
    frame->registers.ref_register_count = 0;

    return ApplyFn(reinterpret_cast<FnPtr>(ptr), self, std::move(params),
                   std::make_index_sequence<sizeof...(Params)>());
  }

  template <size_t... I>
  static Status ApplyFn(
      FnPtr ptr, Owner* self,
      std::tuple<ParamUnpack<typename std::decay<Params>::type>...>&& params,
      std::index_sequence<I...>) {
    return (self->*ptr)(std::get<I>(params)...);
  }
};

}  // namespace packing

template <typename Owner>
struct NativeFunction {
  const char* name;
  void (Owner::*const ptr)();
  Status (*const call)(void (Owner::*ptr)(), Owner* self,
                       iree_vm_stack_t* stack, iree_vm_stack_frame_t* frame,
                       iree_vm_execution_result_t* out_result);
};

template <typename Owner, typename Result, typename... Params>
constexpr NativeFunction<Owner> MakeNativeFunction(
    const char* name, StatusOr<Result> (Owner::*fn)(Params...)) {
  return {name, (void (Owner::*)())fn,
          &packing::DispatchFunctor<Owner, Result, Params...>::Call};
}

template <typename Owner, typename... Params>
constexpr NativeFunction<Owner> MakeNativeFunction(
    const char* name, Status (Owner::*fn)(Params...)) {
  return {name, (void (Owner::*)())fn,
          &packing::DispatchFunctorVoid<Owner, Params...>::Call};
}

}  // namespace vm
}  // namespace iree

#endif  // IREE_VM_MODULE_ABI_PACKING_H_
