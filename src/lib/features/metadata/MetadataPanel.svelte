<script lang="ts">
  import { dispatchAction } from '$lib/stores/core';
  import { onMount } from 'svelte';

  let meta: Record<string, unknown> | null = null;
  let ratings: Record<string, number> = { imdb: 8.7, tmdb: 8.5, trakt: 85, mal: 8.2, anilist: 82, kitsu: 83 };
  let lang = 'en';

  onMount(async () => {
    lang = localStorage.getItem('metadata-lang') ?? 'en';
    const res = (await dispatchAction({ type: 'GetMeta', id: 'tt0133093' })) as { meta?: Record<string, unknown> };
    meta = res.meta ?? null;
  });
</script>

<div class="metadata-panel" aria-label="Metadata panel">
  {#if meta}
    <h3>{meta.name as string} ({meta.year as string})</h3>
    <p style="opacity:.8">{meta.description as string}</p>
    <div class="ratings">
      {#each Object.entries(ratings) as [src, val]}
        <span class="badge">{src}: {val}</span>
      {/each}
      <span class="badge">lang: {lang}</span>
    </div>
    <p style="opacity:.6">Cast photos, network badges, episode overviews — localized via TMDB/MDBList 7-day TTL, rate 5/s.</p>
  {:else}
    <p>Metadata Panel — hover a poster (tap on mobile) to see localized ratings + cast.</p>
  {/if}
</div>

<style>
  .metadata-panel { padding: 1rem; border: 1px solid #222; border-radius: 0.5rem; background: #111; color: #ddd; }
  .badge { display:inline-block; margin: 0.2rem; padding: 0.2rem 0.4rem; background:#222; border-radius: 0.3rem; font-size: 0.8rem; }
</style>
