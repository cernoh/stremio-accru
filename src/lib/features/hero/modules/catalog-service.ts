import { getMovieCatalogUrl, getSeriesCatalogUrl, HERO_LIMITS } from "./config";
import { getCached, setCached } from "./cache";
import { dispatchAction } from "$lib/stores/core";

const PROXIES = [
  (url: string) => `https://proxy.cors.sh/${url}`,
  (url: string) =>
    `https://api.allorigins.win/raw?url=${encodeURIComponent(url)}`,
  (url: string) => url,
];

async function fetchWithProxy(url: string): Promise<unknown[]> {
  for (const wrap of PROXIES) {
    try {
      const proxied = wrap(url);
      const res = await fetch(proxied);
      if (!res.ok) continue;
      const data = (await res.json()) as {
        metas?: unknown[];
        items?: unknown[];
      };
      const items = data.metas ?? data.items ?? [];
      if (items.length) return items as unknown[];
    } catch {
      continue;
    }
  }
  return [];
}

export async function fetchHeroCatalog(
  type: "movie" | "series",
): Promise<unknown[]> {
  const cached = getCached(type);
  if (cached?.length) return cached;

  // Try core first (Cinemeta via stremio-core mock)
  try {
    const res = (await dispatchAction({
      type: "LoadCatalog",
      id: `${type}:popular`,
    })) as {
      catalog?: { items?: unknown[] };
    };
    if (res.catalog?.items?.length) {
      const items = (res.catalog.items as unknown[]).slice(
        0,
        HERO_LIMITS[type],
      );
      setCached(type, items);
      return items;
    }
  } catch {
    // fall through to HTTP
  }

  const url = type === "movie" ? getMovieCatalogUrl() : getSeriesCatalogUrl();
  const items = await fetchWithProxy(url);
  const sliced = items.slice(0, HERO_LIMITS[type]);
  if (sliced.length) setCached(type, sliced);
  return sliced;
}

export async function fetchDailyAnime(): Promise<unknown[]> {
  try {
    const res = await fetch("https://api.jikan.moe/v4/schedules");
    if (!res.ok) return [];
    const data = (await res.json()) as { data?: unknown[] };
    return (data.data ?? []).slice(0, HERO_LIMITS.anime);
  } catch {
    return [];
  }
}
