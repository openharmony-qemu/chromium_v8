// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/heap-inl.h"  // For ToBoolean.
#include "src/logging/counters.h"
#include "src/objects/call-site-info-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

#define CHECK_CALLSITE(frame, method)                                         \
  CHECK_RECEIVER(JSObject, receiver, method);                                 \
  LookupIterator it(isolate, receiver,                                        \
                    isolate->factory()->call_site_info_symbol(),              \
                    LookupIterator::OWN_SKIP_INTERCEPTOR);                    \
  if (it.state() != LookupIterator::DATA) {                                   \
    THROW_NEW_ERROR_RETURN_FAILURE(                                           \
        isolate,                                                              \
        NewTypeError(MessageTemplate::kCallSiteMethod,                        \
                     isolate->factory()->NewStringFromAsciiChecked(method))); \
  }                                                                           \
  auto frame = Cast<CallSiteInfo>(it.GetDataValue())

namespace {

bool IsSecurityTokenCompatible(Isolate* isolate,
                               DirectHandle<CallSiteInfo> frame) {
  Tagged<NativeContext> current_native_context = isolate->raw_native_context();

  auto is_compatible = [&](Tagged<Object> obj) {
    if (!IsJSReceiver(obj)) return true;
    std::optional<Tagged<NativeContext>> maybe_creation_context =
        Cast<JSReceiver>(obj)->GetCreationContext();
    if (maybe_creation_context.has_value()) {
      Tagged<NativeContext> cc = maybe_creation_context.value();
      if (!cc->HasSameSecurityTokenAs(current_native_context)) {
        return false;
      }
    } else {
      return false;
    }
    return true;
  };

  return is_compatible(frame->function()) &&
         is_compatible(frame->receiver_or_instance());
}

Tagged<Object> PositiveNumberOrNull(int value, Isolate* isolate) {
  if (value > 0) return *isolate->factory()->NewNumberFromInt(value);
  return ReadOnlyRoots(isolate).null_value();
}

bool NativeContextIsForShadowRealm(Tagged<NativeContext> native_context) {
  return native_context->scope_info()->scope_type() == SHADOW_REALM_SCOPE;
}

}  // namespace

BUILTIN(CallSitePrototypeGetColumnNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getColumnNumber");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return PositiveNumberOrNull(CallSiteInfo::GetColumnNumber(frame), isolate);
}

BUILTIN(CallSitePrototypeGetEnclosingColumnNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getEnclosingColumnNumber");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return PositiveNumberOrNull(CallSiteInfo::GetEnclosingColumnNumber(frame),
                              isolate);
}

BUILTIN(CallSitePrototypeGetEnclosingLineNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getEnclosingLineNumber");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return PositiveNumberOrNull(CallSiteInfo::GetEnclosingLineNumber(frame),
                              isolate);
}

BUILTIN(CallSitePrototypeGetEvalOrigin) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getEvalOrigin");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return *CallSiteInfo::GetEvalOrigin(frame);
}

BUILTIN(CallSitePrototypeGetFileName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getFileName");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return frame->GetScriptName();
}

BUILTIN(CallSitePrototypeGetFunction) {
  static const char method_name[] = "getFunction";
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, method_name);
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  // ShadowRealms have a boundary: references to outside objects must not exist
  // in the ShadowRealm, and references to ShadowRealm objects must not exist
  // outside the ShadowRealm.
  if (NativeContextIsForShadowRealm(isolate->raw_native_context()) ||
      (IsJSFunction(frame->function()) &&
       NativeContextIsForShadowRealm(
           Cast<JSFunction>(frame->function())->native_context()))) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(
            MessageTemplate::kCallSiteMethodUnsupportedInShadowRealm,
            isolate->factory()->NewStringFromAsciiChecked(method_name)));
  }
  if (frame->IsStrict() ||
      (IsJSFunction(frame->function()) &&
       Cast<JSFunction>(frame->function())->shared()->is_toplevel())) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  isolate->CountUsage(v8::Isolate::kCallSiteAPIGetFunctionSloppyCall);
  return frame->function();
}

BUILTIN(CallSitePrototypeGetFunctionName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getFunctionName");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }

  Tagged<Object> func_obj = frame->function();
  if (IsJSReceiver(func_obj)) {
    // Get function's creation context. Return null if not available.
    Tagged<JSReceiver> function_obj = Cast<JSReceiver>(func_obj);
    std::optional<Tagged<NativeContext>> maybe_creation_context =
        function_obj->GetCreationContext();
    if (!maybe_creation_context.has_value()) {
      return ReadOnlyRoots(isolate).null_value();
    }
  }

  return *CallSiteInfo::GetFunctionName(frame);
}

BUILTIN(CallSitePrototypeGetLineNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getLineNumber");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return PositiveNumberOrNull(CallSiteInfo::GetLineNumber(frame), isolate);
}

BUILTIN(CallSitePrototypeGetMethodName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getMethodName");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return *CallSiteInfo::GetMethodName(frame);
}

BUILTIN(CallSitePrototypeGetPosition) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getPosition");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return Smi::FromInt(CallSiteInfo::GetSourcePosition(frame));
}

BUILTIN(CallSitePrototypeGetPromiseIndex) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getPromiseIndex");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  if (!frame->IsPromiseAll() && !frame->IsPromiseAny() &&
      !frame->IsPromiseAllSettled()) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return Smi::FromInt(CallSiteInfo::GetSourcePosition(frame));
}

BUILTIN(CallSitePrototypeGetScriptHash) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getScriptHash");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return *CallSiteInfo::GetScriptHash(frame);
}

BUILTIN(CallSitePrototypeGetScriptNameOrSourceURL) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getScriptNameOrSourceUrl");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return frame->GetScriptNameOrSourceURL();
}

BUILTIN(CallSitePrototypeGetThis) {
  static const char method_name[] = "getThis";
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, method_name);
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  // ShadowRealms have a boundary: references to outside objects must not exist
  // in the ShadowRealm, and references to ShadowRealm objects must not exist
  // outside the ShadowRealm.
  if (NativeContextIsForShadowRealm(isolate->raw_native_context()) ||
      (IsJSFunction(frame->function()) &&
       NativeContextIsForShadowRealm(
           Cast<JSFunction>(frame->function())->native_context()))) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(
            MessageTemplate::kCallSiteMethodUnsupportedInShadowRealm,
            isolate->factory()->NewStringFromAsciiChecked(method_name)));
  }
  if (frame->IsStrict()) return ReadOnlyRoots(isolate).undefined_value();
  isolate->CountUsage(v8::Isolate::kCallSiteAPIGetThisSloppyCall);
#if V8_ENABLE_WEBASSEMBLY
  if (frame->IsAsmJsWasm()) {
    return frame->GetWasmInstance()
        ->trusted_data(isolate)
        ->native_context()
        ->global_proxy();
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  return frame->receiver_or_instance();
}

BUILTIN(CallSitePrototypeGetTypeName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getTypeName");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return *CallSiteInfo::GetTypeName(frame);
}

BUILTIN(CallSitePrototypeIsAsync) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isAsync");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return isolate->heap()->ToBoolean(frame->IsAsync());
}

BUILTIN(CallSitePrototypeIsConstructor) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isConstructor");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return isolate->heap()->ToBoolean(frame->IsConstructor());
}

BUILTIN(CallSitePrototypeIsEval) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isEval");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return isolate->heap()->ToBoolean(frame->IsEval());
}

BUILTIN(CallSitePrototypeIsNative) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isNative");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return isolate->heap()->ToBoolean(frame->IsNative());
}

BUILTIN(CallSitePrototypeIsPromiseAll) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isPromiseAll");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return isolate->heap()->ToBoolean(frame->IsPromiseAll());
}

BUILTIN(CallSitePrototypeIsToplevel) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isToplevel");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return isolate->heap()->ToBoolean(frame->IsToplevel());
}

BUILTIN(CallSitePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "toString");
  if (!IsSecurityTokenCompatible(isolate, frame)) {
    return *isolate->factory()->empty_string();
  }
  RETURN_RESULT_OR_FAILURE(isolate, SerializeCallSiteInfo(isolate, frame));
}

#undef CHECK_CALLSITE

}  // namespace internal
}  // namespace v8
