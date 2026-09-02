import { writable } from "svelte/store";
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import type { EventCallback } from "@tauri-apps/api/event";

type CoreResult = {
  type?: string;
  state?: {
    addons?: unknown[];
    catalogs?: unknown[];
    [k: string]: unknown;
  };
  catalog?: unknown;
  stream?: { url: string; [k: string]: unknown };
  [k: string]: unknown;
};

export const coreState = writable<unknown>(null);
export const addons = writable<unknown[]>([]);
export const catalogs = writable<unknown[]>([]);

export async function dispatchAction(action: Record<string, unknown>): Promise<CoreResult> {
  const result = (await invoke("dispatch_action", { action })) as CoreResult;
  if (result?.state) coreState.set(result.state);
  if (result?.state?.addons) addons.set(result.state.addons as unknown[]);
  if (result?.state?.catalogs) catalogs.set(result.state.catalogs as unknown[]);
  if (result?.catalog) catalogs.set([result.catalog]);
  if (result?.stream?.url) {
    await invoke("load", { url: result.stream.url, opts: { url: result.stream.url } });
  }
  return result;
}

export async function getState(): Promise<unknown> {
  const state = (await invoke("get_state")) as {
    addons?: unknown[];
    catalogs?: unknown[];
    [k: string]: unknown;
  };
  coreState.set(state);
  if (state?.addons) addons.set(state.addons);
  if (state?.catalogs) catalogs.set(state.catalogs);
  return state;
}

export function initCoreListeners(): void {
  const onNewState: EventCallback<CoreResult> = (e) => {
    if (e.payload?.state) coreState.set(e.payload.state);
  };
  const onEvent: EventCallback<CoreResult> = (_e) => {
    // handle StreamResolved externally if needed
  };
  listen("core:new-state", onNewState);
  listen("core:event", onEvent);
}
