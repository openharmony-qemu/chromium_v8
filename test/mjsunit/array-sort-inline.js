
})();
// comparefn returns an object with valueOf — ToNumber applies.
(function TestComparefnReturnsObject() {
  function cmp(a, b) { return { valueOf() { return a - b; } }; }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3], f());
})();
// =========================================================================
// 19. Non-inlined comparator (builtin / constructor / bound function)
// =========================================================================
// These exercise the path where the comparefn is callable but is NOT a
// user-defined JSFunction the inliner picks up — so the JSCall to comparefn
// stays generic.  All previous tests use `cmp = (a, b) => ...` which gets
// inlined; this section covers the alternative path that originally tripped
// crbug/502439789.
// Array constructor as comparator on PACKED_ELEMENTS.  Returns an array,
// which ToNumber coerces to NaN, so the sort order is unspecified — we just
// require that the call doesn't crash and the result has the right length.
(function TestPackedElementsBuiltinCmp() {
  function f() { return ['c', 'a', 'b'].sort(Array); }
  opt(f);
  assertEquals(3, f().length);
})();
// Same, with PACKED_SMI_ELEMENTS — verifies the SMI path doesn't crash even
// though it doesn't take the same TF reduction the bug originally hit.
(function TestPackedSmiBuiltinCmp() {
  function f() { return [3, 1, 2].sort(Array); }
  opt(f);
  assertEquals(3, f().length);
})();
// Math.max as comparator: returns a real number, so sort ordering is the
// max(a, b) of the two args (always >= 0), which causes a stable-ish order.
// Main goal: exercise SpeculativeToNumber on a Number return value with a
// non-inlined cmp.
(function TestMathMaxCmp() {
  function f() { return ['c', 'a', 'b'].sort(Math.max); }
  opt(f);
  assertEquals(3, f().length);
})();
// Length 0: copy loops have zero trip count.
(function TestEmptyArrayBuiltinCmp() {
  function f() { return [].sort(Array); }
  opt(f);
  assertEquals([], f());
})();
// Length exactly kMaxInlineSortLength (16): boundary of the fast path.
(function TestMaxInlineLengthBuiltinCmp() {
  const a = ['p', 'o', 'n', 'm', 'l', 'k', 'j', 'i',
             'h', 'g', 'f', 'e', 'd', 'c', 'b', 'a'];
  function f() { return a.slice().sort(Array); }
  opt(f);
  assertEquals(16, f().length);
})();
// Comparator that throws on first invocation: exercises the deferred
// throw / Unreachable continuation path that originally hit the DCHECK
// (the failing block in the regression-test trace was on this path).
(function TestThrowingCmp() {
  function badCmp() { throw new Error('boom'); }
  function f() {
    try {
      ['c', 'a', 'b'].sort(badCmp);
      return 'no-throw';
    } catch (e) {
      return e.message;
    }
  }
  opt(f);
  assertEquals('boom', f());
})();
// Comparator that mutates the receiver (length change): verifies the
// post-loop MaybeInsertMapChecks + CheckIf(length unchanged) deopt
// continuations.  The sort operates on a temp copy, so the mutation does
// NOT affect the sort order itself, but the post-sort length check should
// trip on the changed length.
(function TestReceiverMutatingCmp() {
  let arr;
  function mutate(a, b) { arr.length = 1; return 0; }
  function f() {
    arr = ['c', 'a', 'b'];
    arr.sort(mutate);
    return arr.length;
  }
  // Don't assert an exact length — the result depends on whether the
  // length-changed deopt fires before or after the mutating cmp call.
  // We just want to exercise the path without crashing.
  opt(f);
  f();
})();
