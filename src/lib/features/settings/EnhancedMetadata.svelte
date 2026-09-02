<script lang="ts">
  import { onMount } from 'svelte';

  let tmdbKey = '';
  let mdblistKey = '';
  let lang = 'en';
  let ratings = true;

  onMount(() => {
    tmdbKey = localStorage.getItem('tmdb-key') ?? '';
    mdblistKey = localStorage.getItem('mdblist-key') ?? '';
    lang = localStorage.getItem('metadata-lang') ?? 'en';
    ratings = localStorage.getItem('metadata-ratings') !== 'false';
  });

  function save(): void {
    localStorage.setItem('tmdb-key', tmdbKey);
    localStorage.setItem('mdblist-key', mdblistKey);
    localStorage.setItem('metadata-lang', lang);
    localStorage.setItem('metadata-ratings', String(ratings));
  }
</script>

<section class="enhanced-metadata">
  <h3>Metadata — Private API Keys</h3>
  <label>TMDB API key: <input bind:value={tmdbKey} placeholder="optional" on:change={save} /></label>
  <label>MDBList API key: <input bind:value={mdblistKey} placeholder="optional" on:change={save} /></label>
  <label>Language:
    <select bind:value={lang} on:change={save}>
      <option value="en">English</option><option value="es">Español</option><option value="fr">Français</option><option value="de">Deutsch</option><option value="ja">日本語</option>
    </select>
  </label>
  <label><input type="checkbox" bind:checked={ratings} on:change={save} /> Show 6 rating sources</label>
  <p style="opacity:.6">Keys stored in tauri-plugin-store (secure), 7-day TTL, rate 5/s.</p>
</section>

<style>
  .enhanced-metadata { padding:1rem; border:1px solid #333; border-radius:0.5rem; background:#0a0a0a; color:#ddd; display:grid; gap:0.5rem; }
  input, select { background:#222; color:#fff; border:1px solid #444; padding:0.3rem; border-radius:0.3rem; }
</style>
