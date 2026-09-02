export const ROTATION_INTERVAL = 8000;
export const PROGRESSIVE_DAYS_LIMIT = 6;
export const HERO_LIMITS = { movie: 10, series: 10, anime: 20 };

export type HeroSource = "snoak" | "cinemeta" | "mdblist" | "jikan";

export const SOURCES: Record<string, { name: string; url: (type: string) => string }> = {
  snoak: {
    name: "Snoak",
    url: (type) => `https://snoak.example/${type}/trending.json`,
  },
  cinemeta: {
    name: "Cinemeta",
    url: (type) => `https://v3-cinemeta.strem.io/catalog/${type}/top.json`,
  },
  mdblist: {
    name: "MDBList",
    url: () => {
      const idx = Number(localStorage.getItem("hero-movie-source-index") ?? "0");
      const custom = localStorage.getItem("hero-mdblist-custom");
      if (custom) return custom;
      return `https://mdblist.com/lists/snoak/${idx}/json`;
    },
  },
  jikan: {
    name: "Jikan",
    url: () => "https://api.jikan.moe/v4/schedules",
  },
};

export function getMovieCatalogUrl(): string {
  const src = (localStorage.getItem("hero-movie-source") as HeroSource) ?? "cinemeta";
  return SOURCES[src]?.url("movie") ?? SOURCES.cinemeta.url("movie");
}

export function getSeriesCatalogUrl(): string {
  const src = (localStorage.getItem("hero-series-source") as HeroSource) ?? "cinemeta";
  return SOURCES[src]?.url("series") ?? SOURCES.cinemeta.url("series");
}
