import { beforeEach, describe, expect, it } from "vitest";
import {
  getMovieCatalogUrl,
  getSeriesCatalogUrl,
  HERO_LIMITS,
  PROGRESSIVE_DAYS_LIMIT,
  ROTATION_INTERVAL,
  SOURCES,
} from "./config";

describe("hero config", () => {
  beforeEach(() => localStorage.clear());

  it("exports expected constants", () => {
    expect(ROTATION_INTERVAL).toBe(8000);
    expect(PROGRESSIVE_DAYS_LIMIT).toBe(6);
    expect(HERO_LIMITS.movie).toBe(10);
    expect(HERO_LIMITS.series).toBe(10);
    expect(HERO_LIMITS.anime).toBe(20);
  });

  it("has 4 known sources", () => {
    expect(Object.keys(SOURCES)).toEqual(
      expect.arrayContaining(["snoak", "cinemeta", "mdblist", "jikan"]),
    );
  });

  it("defaults to cinemeta when no localStorage", () => {
    expect(getMovieCatalogUrl()).toBe(SOURCES.cinemeta.url("movie"));
    expect(getSeriesCatalogUrl()).toBe(SOURCES.cinemeta.url("series"));
  });

  it("respects hero-movie-source localStorage", () => {
    localStorage.setItem("hero-movie-source", "snoak");
    expect(getMovieCatalogUrl()).toBe(
      "https://snoak.example/movie/trending.json",
    );
    localStorage.setItem("hero-movie-source", "jikan");
    expect(getMovieCatalogUrl()).toBe("https://api.jikan.moe/v4/schedules");
  });

  it("falls back to cinemeta on unknown source", () => {
    localStorage.setItem("hero-movie-source", "unknown-source");
    expect(getMovieCatalogUrl()).toBe(SOURCES.cinemeta.url("movie"));
  });

  it("mdblist returns custom url when hero-mdblist-custom set", () => {
    localStorage.setItem("hero-movie-source", "mdblist");
    localStorage.setItem(
      "hero-mdblist-custom",
      "https://custom.example/list.json",
    );
    expect(getMovieCatalogUrl()).toBe("https://custom.example/list.json");
  });

  it("mdblist returns list url with index from localStorage", () => {
    localStorage.setItem("hero-movie-source", "mdblist");
    localStorage.removeItem("hero-mdblist-custom");
    localStorage.setItem("hero-movie-source-index", "3");
    expect(getMovieCatalogUrl()).toBe("https://mdblist.com/lists/snoak/3/json");
  });

  it("series url respects its own source key", () => {
    localStorage.setItem("hero-series-source", "snoak");
    expect(getSeriesCatalogUrl()).toBe(
      "https://snoak.example/series/trending.json",
    );
    // movie source shouldn't affect series when series source not set? movie is separate key
    localStorage.setItem("hero-movie-source", "jikan");
    localStorage.setItem("hero-series-source", "cinemeta");
    expect(getSeriesCatalogUrl()).toBe(SOURCES.cinemeta.url("series"));
  });
});
