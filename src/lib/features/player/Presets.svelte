<script lang="ts">
  import { invoke } from '@tauri-apps/api/core';
  import { listen } from '@tauri-apps/api/event';
  import { onMount } from 'svelte';

  let shader = 'Off';
  let visual: string = 'Kai';
  let audio: string = 'Off';
  let hdr = false;
  let svp = false;
  let skipMsg = '';
  let thumb = '';

  onMount(() => {
    listen('player:skip-toast', (e: { payload: { label: string; start: number; end: number } }) => {
      skipMsg = `${e.payload.label} ${e.payload.start}-${e.payload.end}s`;
      setTimeout(() => (skipMsg = ''), 3000);
    });
    listen('player:thumbnail', (e: { payload: { url: string } }) => {
      thumb = e.payload.url;
    });
  });

  async function setShader(p: string): Promise<void> {
    shader = p;
    await invoke('set_shader_preset', { preset: p });
  }
  async function setVisual(p: string): Promise<void> {
    visual = p;
    await invoke('set_visual_profile', { profile: p });
  }
  async function setAudio(p: string): Promise<void> {
    audio = p;
    await invoke('set_audio_preset', { preset: p });
  }
  async function toggleHdr(): Promise<void> {
    hdr = !hdr;
    await invoke('set_hdr', { enabled: hdr });
  }
  async function toggleSvp(): Promise<void> {
    svp = !svp;
    await invoke('set_svp', { enabled: svp });
  }
  async function testSkip(): Promise<void> {
    await invoke('request_skip', { timePos: 10, duration: 1500 });
  }
  async function testThumb(): Promise<void> {
    await invoke('request_thumbnail', { timePos: 42.5 });
  }
</script>

<section class="presets" aria-label="Player presets and automation">
  <h3>Presets & Automation — M4</h3>
  <div class="row">
    <span>Anime4K:</span>
    {#each ['Off', 'Optimized', 'Fast', 'HQ'] as p}
      <button on:click={() => setShader(p)} class:active={shader === p}>{p}</button>
    {/each}
  </div>
  <div class="row">
    <span>Visual:</span>
    {#each ['Kai', 'Vivid', 'Original'] as p}
      <button on:click={() => setVisual(p)} class:active={visual === p}>{p}</button>
    {/each}
  </div>
  <div class="row">
    <span>Audio:</span>
    {#each ['Off', 'Night', 'Voice'] as p}
      <button on:click={() => setAudio(p)} class:active={audio === p}>{p}</button>
    {/each}
  </div>
  <div class="row">
    <button on:click={toggleHdr}>HDR: {hdr ? 'ON (passthrough)' : 'OFF (tonemap)'}</button>
    <button on:click={toggleSvp}>SVP: {svp ? 'ON' : 'OFF'} (desktop-only, gated)</button>
  </div>
  <div class="row">
    <button on:click={testSkip}>Test Skip Toast</button>
    <span>{skipMsg}</span>
  </div>
  <div class="row">
    <button on:click={testThumb}>Test Thumbnail</button>
    <span>{thumb}</span>
  </div>
  <p style="opacity:.6">Hi-Fi Audio (Cinema/Anime/Night), Visual Kai/Vivid/Original, HDR passthrough, Anime4K/SVP desktop-gated, Skip (IntroDB→chapters→filter), Smart Track Selector, Thumbfast.</p>
</section>

<style>
  .presets { padding:1rem; border:1px solid #333; border-radius:0.5rem; background:#0a0a0a; color:#ddd; display:grid; gap:0.5rem; }
  .row { display:flex; gap:0.5rem; align-items:center; flex-wrap:wrap; }
  button.active { background:#6a5cff; color:#fff; }
</style>
