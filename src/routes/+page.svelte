<script lang="ts">
  import HeroBanner from '$lib/features/hero/HeroBanner.svelte';
  import MetadataPanel from '$lib/features/metadata/MetadataPanel.svelte';
  import HiddenNav from '$lib/features/navigation/HiddenNav.svelte';
  import Player from '$lib/features/player/Player.svelte';
  import Presets from '$lib/features/player/Presets.svelte';
  import '$lib/theme/oled.css';
</script>

<HiddenNav />

<main>
  <h1>Stremio Accru — M4 Presets & Automation</h1>
  <p>Hero, Metadata, Hidden Nav/OLED, Player backend, Presets (HDR/SVP/Anime4K/Audio/Visual), Skip/Tracks/Thumbs.</p>
  <HeroBanner />
  <MetadataPanel />
  <Player />
  <Presets />
</main>

<style>
  main { padding: 2rem; display: grid; gap: 1rem; }
</style>
