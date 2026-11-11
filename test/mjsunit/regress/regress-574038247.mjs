// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ns from "./modules-skip-574038247-m.mjs";
import { a } from "./modules-skip-574038247-m_no_ns.mjs";

assertEquals(1, a);

function read(o) {
  return o.a + o.b + o.c + o.d + o.e;
}

for (let i = 0; i < 12; i++) {
  assertEquals(5, read(ns));
}

await assertThrowsAsync(import("./modules-skip-574038247-trigger.mjs"), SyntaxError);
assertEquals(5, read(ns));

await assertThrowsAsync(import("./modules-skip-574038247-trigger_no_ns.mjs"), SyntaxError);
await assertThrowsAsync(import("./modules-skip-574038247-import_x_from_m_no_ns.mjs"), SyntaxError);
