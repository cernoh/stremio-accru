import { PROGRESSIVE_DAYS_LIMIT } from "./config";

const KEY_MOVIE = "heroMovieTitlesCache";
const KEY_SERIES = "heroSeriesTitlesCache";
const KEY_TS = "heroGlobalTimestamp";

type CacheEntry = { day: number; items: unknown[]; ts: number };

function load(key: string): CacheEntry | null {
  try {
    const raw = localStorage.getItem(key);
    return raw ? (JSON.parse(raw) as CacheEntry) : null;
  } catch {
    return null;
  }
}

function save(key: string, entry: CacheEntry): void {
  localStorage.setItem(key, JSON.stringify(entry));
}

export function getCached(type: "movie" | "series"): unknown[] | null {
  const key = type === "movie" ? KEY_MOVIE : KEY_SERIES;
  const entry = load(key);
  if (!entry) return null;
  const ageDays = (Date.now() - entry.ts) / 86400000;
  if (ageDays > PROGRESSIVE_DAYS_LIMIT) return null;
  return entry.items;
}

export function setCached(type: "movie" | "series", items: unknown[]): void {
  const key = type === "movie" ? KEY_MOVIE : KEY_SERIES;
  save(key, { day: new Date().getDay(), items, ts: Date.now() });
  localStorage.setItem(KEY_TS, String(Date.now()));
}

export function isStale(): boolean {
  const ts = Number(localStorage.getItem(KEY_TS) ?? "0");
  return Date.now() - ts > 86400000;
}
