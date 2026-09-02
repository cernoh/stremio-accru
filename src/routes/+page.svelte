<script lang="ts">
  import HeroBanner from '$lib/features/hero/HeroBanner.svelte';
  import MetadataPanel from '$lib/features/metadata/MetadataPanel.svelte';
  import HiddenNav from '$lib/features/navigation/HiddenNav.svelte';
  import Player from '$lib/features/player/Player.svelte';
  import '$lib/theme/oled.css';
</script>

<HiddenNav />

<main>
  <h1>Stremio Accru — M3 UI</h1>
  <p>See <code>docs/PLAN.md</code>. Hero (8s/6-day), Metadata (6 ratings), Hidden Nav/OLED, Player backend ready.</p>
  <HeroBanner />
  <MetadataPanel />
  <Player />
</main>

<style>
  main { padding: 2rem; display: grid; gap: 1rem; }
</style>
