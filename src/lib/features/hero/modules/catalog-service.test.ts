import { beforeEach, describe, expect, it, vi } from "vitest";
import { clearMocks, mockIPC } from "@tauri-apps/api/mocks";

describe("hero catalog-service fetchHeroCatalog", () => {
  beforeEach(() => {
    localStorage.clear();
    clearMocks();
    vi.restoreAllMocks();
  });

  it("returns cached items without hitting core", async () => {
    const { setCached } = await import("./cache");
    const cached = [{ id: "tt1", name: "Cached" }];
    setCached("movie", cached);

    const { fetchHeroCatalog } = await import("./catalog-service");
    // Even if core would return different, cache wins
    mockIPC(() => {
      throw new Error("should not be called when cached");
    });
    const result = await fetchHeroCatalog("movie");
    expect(result).toEqual(cached);
  });

  it("fetches via dispatchAction when not cached", async () => {
    const mockCatalog = {
      id: "movie:popular",
      items: [{ id: "tt2", name: "FromCore" }, { id: "tt3", name: "B" }],
    };
    mockIPC((cmd) => {
      if (cmd === "dispatch_action") {
        return {
          type: "NewState",
          catalog: mockCatalog,
          state: { catalogs: [mockCatalog] },
        };
      }
    });

    const { fetchHeroCatalog } = await import("./catalog-service");
    const result = await fetchHeroCatalog("movie");
    // HERO_LIMITS.movie is 10, so all items returned
    expect(result).toEqual(mockCatalog.items);
    // should have cached
    const { getCached } = await import("./cache");
    expect(getCached("movie")).toEqual(mockCatalog.items);
  });

  it("falls back to fetchWithProxy when dispatchAction throws", async () => {
    mockIPC((cmd) => {
      if (cmd === "dispatch_action") throw new Error("core down");
    });
    const fakeItems = [{ id: "tt-proxy", name: "Proxy Item" }];
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => ({
        ok: true,
        json: async () => ({ metas: fakeItems }),
      } as Response)),
    );

    const { fetchHeroCatalog } = await import("./catalog-service");
    const result = await fetchHeroCatalog("series");
    expect(result).toEqual(fakeItems);
  });

  it("fetchDailyAnime returns [] on fetch failure", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => {
        throw new Error("network");
      }),
    );
    const { fetchDailyAnime } = await import("./catalog-service");
    await expect(fetchDailyAnime()).resolves.toEqual([]);
  });

  it("fetchDailyAnime returns sliced data on success", async () => {
    const fakeData = Array.from({ length: 25 }, (_, i) => ({ id: `${i}` }));
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => ({
        ok: true,
        json: async () => ({ data: fakeData }),
      } as Response)),
    );
    const { fetchDailyAnime } = await import("./catalog-service");
    const result = await fetchDailyAnime();
    expect(result).toHaveLength(20); // HERO_LIMITS.anime
    expect((result[0] as { id: string }).id).toBe("0");
  });
});
