<script lang="ts">
  import { invoke } from '@tauri-apps/api/core';
  import { onMount } from 'svelte';

  let shader: string = 'Off';
  let svp = false;
  let hdr = false;
  let visual: string = 'Kai';
  let audio: string = 'Off';
  let vulkan = false;
  let panscan = 0;

  onMount(() => {
    shader = localStorage.getItem('mpv-shader') ?? 'Off';
    svp = localStorage.getItem('mpv-svp') === 'true';
    hdr = localStorage.getItem('mpv-hdr') === 'true';
    visual = localStorage.getItem('mpv-visual') ?? 'Kai';
    audio = localStorage.getItem('mpv-audio') ?? 'Off';
  });

  async function updateShader(v: string): Promise<void> {
    shader = v;
    localStorage.setItem('mpv-shader', v);
    await invoke('set_shader_preset', { preset: v });
  }
  async function updateSvp(): Promise<void> {
    svp = !svp;
    localStorage.setItem('mpv-svp', String(svp));
    await invoke('set_svp', { enabled: svp });
  }
  async function updateHdr(): Promise<void> {
    hdr = !hdr;
    localStorage.setItem('mpv-hdr', String(hdr));
    await invoke('set_hdr', { enabled: hdr });
  }
  async function updateVisual(v: string): Promise<void> {
    visual = v;
    localStorage.setItem('mpv-visual', v);
    await invoke('set_visual_profile', { profile: v });
  }
  async function updateAudio(v: string): Promise<void> {
    audio = v;
    localStorage.setItem('mpv-audio', v);
    await invoke('set_audio_preset', { preset: v });
  }
  async function updatePanscan(v: number): Promise<void> {
    panscan = v;
    localStorage.setItem('mpv-panscan', String(v));
    await invoke('set_property', { key: 'panscan', value: v });
  }
</script>

<section class="mpv-settings">
  <h3>Player Settings — Instant Apply</h3>
  <div class="grid">
    <label>Shader:
      <select value={shader} on:change={(e) => updateShader((e.target as HTMLSelectElement).value)}>
        <option>Off</option><option>Optimized</option><option>Fast</option><option>HQ</option>
      </select>
    </label>
    <label><input type="checkbox" checked={svp} on:change={updateSvp} /> SVP 48/60 (desktop)</label>
    <label><input type="checkbox" checked={hdr} on:change={updateHdr} /> HDR passthrough</label>
    <label><input type="checkbox" checked={vulkan} on:change={() => (vulkan = !vulkan)} /> Vulkan async</label>
    <label>Visual:
      <select value={visual} on:change={(e) => updateVisual((e.target as HTMLSelectElement).value)}>
        <option>Kai</option><option>Vivid</option><option>Original</option>
      </select>
    </label>
    <label>Audio:
      <select value={audio} on:change={(e) => updateAudio((e.target as HTMLSelectElement).value)}>
        <option>Off</option><option>Night</option><option>Voice</option>
      </select>
    </label>
    <label>Panscan: <input type="range" min="0" max="1" step="0.1" value={panscan} on:input={(e) => updatePanscan(Number((e.target as HTMLInputElement).value))} /> {panscan}</label>
  </div>
  <p style="opacity:.6">All changes apply instantly to playing content via mpv-bridge (see profile-manager.lua).</p>
</section>

<style>
  .mpv-settings { padding:1rem; border:1px solid #333; border-radius:0.5rem; background:#0a0a0a; color:#ddd; }
  .grid { display:grid; gap:0.5rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
</style>
