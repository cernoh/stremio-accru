import { writable } from 'svelte/store';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';

export const currentUrl = writable<string | null>(null);
export const timePos = writable<number>(0);
export const isPaused = writable<boolean>(false);

export async function load(url: string) {
  await invoke('load', { url, opts: { url } });
}

export async function setProperty(key: string, value: unknown) {
  await invoke('set_property', { key, value });
}

export async function observe(key: string) {
  await invoke('observe', { key });
}

export async function sendCommand(cmd: string, args: string[] = []) {
  await invoke('command', { cmd, args });
}

export function initListeners() {
  listen('player:property-changed', (e: { payload: { key: string; value: unknown } }) => {
    const { key, value } = e.payload;
    if (key === 'time-pos' && typeof value === 'number') timePos.set(value);
    if (key === 'path' && typeof value === 'string') currentUrl.set(value);
    if (key === 'pause' && typeof value === 'boolean') isPaused.set(value);
  });
  listen('player:playback-ended', () => {
    currentUrl.set(null);
    timePos.set(0);
  });
}
