import { beforeEach, describe, expect, it, vi } from "vitest";
import { getCached, isStale, setCached } from "./cache";
import { PROGRESSIVE_DAYS_LIMIT } from "./config";

describe("hero cache", () => {
  beforeEach(() => {
    localStorage.clear();
    vi.restoreAllMocks();
  });

  it("returns null when nothing cached", () => {
    expect(getCached("movie")).toBeNull();
    expect(getCached("series")).toBeNull();
  });

  it("setCached then getCached round-trips", () => {
    const items = [{ id: "tt1", name: "A" }, { id: "tt2", name: "B" }];
    setCached("movie", items);
    expect(getCached("movie")).toEqual(items);
    // series remains null
    expect(getCached("series")).toBeNull();
  });

  it("separate keys for movie vs series", () => {
    setCached("movie", [{ id: "m" }]);
    setCached("series", [{ id: "s" }]);
    expect(getCached("movie")).toEqual([{ id: "m" }]);
    expect(getCached("series")).toEqual([{ id: "s" }]);
  });

  it("returns null when stale (> PROGRESSIVE_DAYS_LIMIT)", () => {
    const items = [{ id: "x" }];
    setCached("movie", items);
    // advance time beyond limit
    const future = Date.now() + (PROGRESSIVE_DAYS_LIMIT + 1) * 86400000;
    vi.spyOn(Date, "now").mockReturnValue(future);
    expect(getCached("movie")).toBeNull();
  });

  it("still valid just before expiry", () => {
    const items = [{ id: "y" }];
    setCached("movie", items);
    const almost = Date.now() + (PROGRESSIVE_DAYS_LIMIT - 0.5) * 86400000;
    // getCached checks age from now - entry.ts ; if entry.ts is recent, almost is still within window?
    // Need to mock entry ts to be in past. Instead set entry with old ts via localStorage directly.
    const raw = JSON.parse(localStorage.getItem("heroMovieTitlesCache")!);
    raw.ts = almost - (PROGRESSIVE_DAYS_LIMIT - 0.5) * 86400000 + 1000; // keep recent
    localStorage.setItem("heroMovieTitlesCache", JSON.stringify(raw));
    vi.spyOn(Date, "now").mockReturnValue(almost);
    expect(getCached("movie")).toEqual(items);
  });

  it("isStale reflects global timestamp", () => {
    expect(isStale()).toBe(true); // nothing cached => 0 => stale
    setCached("movie", []);
    expect(isStale()).toBe(false);
    const future = Date.now() + 86400001;
    vi.spyOn(Date, "now").mockReturnValue(future);
    expect(isStale()).toBe(true);
  });

  it("handles corrupted localStorage gracefully", () => {
    localStorage.setItem("heroMovieTitlesCache", "not-json");
    expect(getCached("movie")).toBeNull();
  });

  it("overwrites previous cache", () => {
    setCached("movie", [{ id: "1" }]);
    setCached("movie", [{ id: "2" }, { id: "3" }]);
    expect(getCached("movie")).toEqual([{ id: "2" }, { id: "3" }]);
  });
});
