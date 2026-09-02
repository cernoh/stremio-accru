// Vitest setup — polyfills required for Tauri mockIPC + Svelte in jsdom
import "@testing-library/jest-dom/vitest";
import { randomFillSync } from "crypto";

// jsdom lacks WebCrypto getRandomValues — Tauri @tauri-apps/api/mocks needs it
if (typeof window !== "undefined" && !window.crypto?.getRandomValues) {
  Object.defineProperty(window, "crypto", {
    value: {
      // @ts-ignore
      getRandomValues: (buffer: Uint8Array) => randomFillSync(buffer),
    },
  });
}

// Ensure localStorage and fetch exist in jsdom (they do, but guard for node env)
if (typeof globalThis.fetch === "undefined") {
  // @ts-ignore
  globalThis.fetch = async () => ({ ok: false, json: async () => ({}) }) as Response;
}
