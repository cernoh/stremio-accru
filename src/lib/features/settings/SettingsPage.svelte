<script lang="ts">
  import MpvSettings from './MpvSettings.svelte';
  import EnhancedMetadata from './EnhancedMetadata.svelte';
  import { onMount } from 'svelte';

  let oled = false;
  let autoFullscreen = false;
  let shortcuts: string = 'F1-F12, `';

  onMount(() => {
    oled = localStorage.getItem('stremio-oled-theme-enabled') === 'true';
    autoFullscreen = localStorage.getItem('auto-fullscreen') === 'true';
    shortcuts = localStorage.getItem('custom-shortcuts') ?? 'F1-F12, `';
  });

  function toggleOled(): void {
    oled = !oled;
    localStorage.setItem('stremio-oled-theme-enabled', String(oled));
    document.documentElement.dataset.oled = String(oled);
  }
  function toggleAutoFullscreen(): void {
    autoFullscreen = !autoFullscreen;
    localStorage.setItem('auto-fullscreen', String(autoFullscreen));
  }
</script>

<div class="settings-page">
  <h2>Settings — No More .conf Editing</h2>
  <MpvSettings />
  <EnhancedMetadata />
  <section class="card">
    <h3>Theme & Navigation</h3>
    <label><input type="checkbox" checked={oled} on:change={toggleOled} /> OLED Pure Black (#000)</label>
    <label><input type="checkbox" checked={autoFullscreen} on:change={toggleAutoFullscreen} /> Auto Fullscreen (living-room)</label>
    <label>Shortcuts: <input bind:value={shortcuts} on:change={() => localStorage.setItem('custom-shortcuts', shortcuts)} /></label>
  </section>
  <p style="opacity:.6">First-time wizard shown on first launch; all settings persist via tauri-plugin-store + localStorage and apply instantly.</p>
</div>

<style>
  .settings-page { display:grid; gap:1rem; padding:1rem; }
  .card { padding:1rem; border:1px solid #333; border-radius:0.5rem; background:#0a0a0a; color:#ddd; display:grid; gap:0.5rem; }
</style>
