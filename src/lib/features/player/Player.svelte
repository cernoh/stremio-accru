<script lang="ts">
  import { onMount } from 'svelte';
  import { currentUrl, timePos, load, observe, initListeners } from '$lib/stores/player';

  let url = '';
  let pos = 0;
  let cur: string | null = null;

  currentUrl.subscribe(v => cur = v);
  timePos.subscribe(v => pos = v);

  onMount(() => {
    initListeners();
    observe('time-pos');
    observe('path');
  });

  async function handleLoad() {
    if (url) await load(url);
  }
</script>

<section class="player" aria-label="Player (stub)">
  <h3>Player — M1 backend ready</h3>
  <p>Current: {cur ?? '—'} — pos {pos.toFixed(1)}s</p>
  <input bind:value={url} placeholder="https://…/video.mp4" style="width:60%" />
  <button on:click={handleLoad}>Load (invoke)</button>
  <p style="opacity:.6">Desktop uses mock DesktopPlayer (libmpv2 placeholder); mobile gated. Events stream via <code>player:property-changed</code> / <code>time-pos</code>.</p>
</section>
