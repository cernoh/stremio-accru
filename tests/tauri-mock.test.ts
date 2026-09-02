import { beforeAll, beforeEach, describe, expect, it, vi } from "vitest";
import { randomFillSync } from "crypto";
import { clearMocks, mockIPC, mockWindows } from "@tauri-apps/api/mocks";
import { invoke } from "@tauri-apps/api/core";
import { emit, listen } from "@tauri-apps/api/event";
import { getCurrentWindow } from "@tauri-apps/api/window";

beforeAll(() => {
  Object.defineProperty(window, "crypto", {
    value: {
      getRandomValues: (buffer: Uint8Array) => randomFillSync(buffer),
    },
  });
});

describe("tauri mockIPC — @tauri-apps/api/mocks (docs pattern)", () => {
  beforeEach(() => clearMocks());

  it("mockIPC intercepts invoke('add') as per Tauri docs", async () => {
    mockIPC((cmd, args) => {
      if (cmd === "add") {
        const typed = args as { a: number; b: number };
        return typed.a + typed.b;
      }
    });
    await expect(invoke("add", { a: 12, b: 15 })).resolves.toBe(27);
  });
  it("spy on invoke via vi.spyOn", async () => {
    mockIPC((cmd, args) => {
      if (cmd === "add") {
        const typed = args as { a: number; b: number };
        return typed.a + typed.b;
      }
    });
    // @ts-ignore: Tauri internals mocked by mockIPC
    // deno-lint-ignore no-window -- jsdom window required for Tauri mock internals
    const internals = window.__TAURI_INTERNALS__ as unknown as {
      invoke: typeof invoke;
    };
    const spy = vi.spyOn(internals, "invoke");
    await expect(invoke("add", { a: 5, b: 7 })).resolves.toBe(12);
    expect(spy).toHaveBeenCalled();
  });

  it("mocks stremio-accru commands dispatch_action / get_state / load", async () => {
    mockIPC((cmd, args) => {
      if (cmd === "dispatch_action") {
        const action = (args as { action: { type: string } }).action;
        if (action?.type === "LoadCatalog") {
          return {
            type: "NewState",
            catalog: {
              id: "movie:popular",
              items: [{ id: "tt1", name: "Mock Movie" }],
            },
            state: { catalogs: [{ id: "movie:popular" }], addons: [] },
          };
        }
        return { type: "NewState", state: { catalogs: [], addons: [] } };
      }
      if (cmd === "get_state") {
        return { catalogs: [{ id: "movie:popular" }], addons: [], ctx: {} };
      }
      if (cmd === "load") return null;
    });

    const res = (await invoke("dispatch_action", {
      action: { type: "LoadCatalog", id: "movie:popular" },
    })) as { type: string; catalog: { items: { name: string }[] } };
    expect(res.type).toBe("NewState");
    expect(res.catalog.items[0].name).toBe("Mock Movie");

    const state = (await invoke("get_state")) as { catalogs: unknown[] };
    expect(state.catalogs).toHaveLength(1);

    await expect(
      invoke("load", {
        url: "https://example.com/video.mp4",
        opts: { url: "https://example.com/video.mp4" },
      }),
    ).resolves.toBeNull();
  });

  it("mockIPC can simulate error", async () => {
    mockIPC((cmd) => {
      if (cmd === "failing_command") throw new Error("mock error");
    });
    await expect(invoke("failing_command")).rejects.toThrow();
  });
});

describe("tauri mockWindows", () => {
  beforeEach(() => clearMocks());

  it("mockWindows creates fake window labels", () => {
    mockWindows("main", "second", "third");
    const current = getCurrentWindow();
    expect(current.label).toBe("main");
  });
});

describe("tauri event mocking (shouldMockEvents)", () => {
  it("mockIPC with shouldMockEvents emits/listens", async () => {
    clearMocks();
    mockIPC(() => {}, { shouldMockEvents: true });
    const handler = vi.fn();
    await listen("test-event", handler);
    await emit("test-event", { foo: "bar" });
    expect(handler).toHaveBeenCalledWith(
      expect.objectContaining({ event: "test-event", payload: { foo: "bar" } }),
    );
  });
});
