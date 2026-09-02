<script lang="ts">
  import { onMount, onDestroy } from 'svelte';
  import { ROTATION_INTERVAL } from './modules/config';
  import { fetchHeroCatalog, fetchDailyAnime } from './modules/catalog-service';

  let items: unknown[] = [];
  let anime: unknown[] = [];
  let idx = 0;
  let timer: ReturnType<typeof setInterval> | null = null;

  function current(): unknown {
    const all = [...items, ...anime];
    return all[idx % Math.max(all.length, 1)];
  }

  onMount(async () => {
    const [movies, series, daily] = await Promise.all([
      fetchHeroCatalog('movie'),
      fetchHeroCatalog('series'),
      fetchDailyAnime(),
    ]);
    items = [...(movies as unknown[]), ...(series as unknown[])].slice(0, 10);
    anime = daily as unknown[];
    timer = setInterval(() => {
      idx = (idx + 1) % Math.max([...items, ...anime].length, 1);
    }, ROTATION_INTERVAL);
  });

  onDestroy(() => {
    if (timer) clearInterval(timer);
  });
</script>

<section class="hero-banner" aria-label="Hero banner">
  {#if items.length || anime.length}
    <div class="hero-track">
      {#key idx}
        <div class="hero-card">
          <pre style="font-size:0.8rem; white-space:pre-wrap">{JSON.stringify(current(), null, 2)}</pre>
          <p style="opacity:.6">Hero • {idx + 1} / {items.length + anime.length} • 8s rotation • 6-day cache • MDBList/custom + Jikan daily</p>
        </div>
      {/key}
    </div>
  {:else}
    <p>Hero Banner — loading catalogs…</p>
  {/if}
</section>

<style>
  .hero-banner { padding: 1rem; border: 1px solid #333; border-radius: 0.75rem; background: #0c0b11; color: #e6e6ff; }
  .hero-card { transition: opacity 300ms; }
</style>
