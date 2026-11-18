// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0
// Flags: --max-maglev-inlined-bytecode-size-small-with-heapnum-in-out=0

// V8 14.4 uses the small-function thresholds for eager inlining and
// AllocateHeapNumber() for an explicitly allocated HeapNumber containing zero.

function zero() {
  return 0;
}
%PrepareFunctionForOptimization(zero);

// CheckedNumberToInt32 must not propagate Smi range back to its tagged input.
let v_int32 = 0;
v_int32 = %AllocateHeapNumber();
let v_smi1 = 1;
let v_smi2 = 1;

function foo_int32(x) {
  v_int32 = x;
  let y = x + zero();
  v_smi1 = y;
  v_smi2 = x;
}

function read_smi2() {
  return v_smi2 + 1;
}

%PrepareFunctionForOptimization(foo_int32);
%PrepareFunctionForOptimization(read_smi2);
foo_int32(0);
assertEquals(1, read_smi2());
%OptimizeMaglevOnNextCall(read_smi2);
assertEquals(1, read_smi2());

// Tenure v_smi2's ContextCell before allocating a young HeapNumber(0.0) so an
// elided write barrier also leaves an unrecorded old-to-new pointer.
gc();
let hn0_int32 = %AllocateHeapNumber();

%OptimizeMaglevOnNextCall(foo_int32);
foo_int32(hn0_int32);
gc({type: 'minor'});
assertEquals(1, read_smi2());
assertEquals(0, v_smi1);
assertEquals(0, v_smi2);

// CheckedNumberToFloat64 must not propagate Smi range back to its tagged input.
let v_f64 = 0;
v_f64 = 1.5;
let v_f64_smi1 = 1;
let v_f64_smi2 = 1;

function foo_f64(x) {
  v_f64 = x;
  let y = x + zero();
  v_f64_smi1 = y;
  v_f64_smi2 = x;
}

function read_f64_smi2() {
  return v_f64_smi2 + 1;
}

%PrepareFunctionForOptimization(foo_f64);
%PrepareFunctionForOptimization(read_f64_smi2);
foo_f64(0);
assertEquals(1, read_f64_smi2());
%OptimizeMaglevOnNextCall(read_f64_smi2);
assertEquals(1, read_f64_smi2());

gc();
let hn0_f64 = %AllocateHeapNumber();

%OptimizeMaglevOnNextCall(foo_f64);
foo_f64(hn0_f64);
gc({type: 'minor'});
assertEquals(1, read_f64_smi2());
assertEquals(0, v_f64_smi1);
assertEquals(0, v_f64_smi2);
