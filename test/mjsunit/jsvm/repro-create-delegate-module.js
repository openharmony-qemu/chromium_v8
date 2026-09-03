import * as ns from "./repro-create-delegate-module.js";

export let caughtError;

try {
  __Longque__.createDelegate(ns);
} catch (error) {
  caughtError = error;
}

export let boom = 1;
