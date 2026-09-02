// Vitest setup — polyfills required for Tauri mockIPC + Svelte in jsdom
import "@testing-library/jest-dom/vitest";
import { randomFillSync } from "crypto";

// jsdom lacks WebCrypto getRandomValues — Tauri @tauri-apps/api/mocks needs it
// deno-lint-ignore no-window -- jsdom window required for vitest WebCrypto polyfill
if (typeof window !== "undefined" && !window.crypto?.getRandomValues) {
  Object.defineProperty(window, "crypto", {
    value: {
      // @ts-ignore: randomFillSync polyfill for jsdom WebCrypto missing getRandomValues
      getRandomValues: (buffer: Uint8Array) => randomFillSync(buffer),
    },
  });
}

// Ensure localStorage and fetch exist in jsdom (they do, but guard for node env)
if (typeof globalThis.fetch === "undefined") {
  // @ts-ignore: fetch polyfill for jsdom guard - ensure fetch exists in Node env
  // deno-lint-ignore require-await -- fetch mock must return Promise per API
  globalThis.fetch = async () =>
    // deno-lint-ignore require-await -- json mock must return Promise per Response API
    ({ ok: false, json: async () => ({}) }) as Response;
}
