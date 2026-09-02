<script lang="ts">
  import { onMount } from 'svelte';

  let step = 1;
  let visible = false;

  onMount(() => {
    const done = localStorage.getItem('wizard-done');
    if (!done) visible = true;
  });

  function next(): void {
    step += 1;
    if (step > 3) {
      localStorage.setItem('wizard-done', 'true');
      visible = false;
    }
  }
</script>

{#if visible}
  <div class="wizard" role="dialog" aria-label="First-time setup wizard">
    <h2>Welcome to Stremio Accru — Step {step}/3</h2>
    {#if step === 1}
      <p>Choose your language and theme. OLED pure-black for AMOLED, auto-fullscreen for TV.</p>
    {:else if step === 2}
      <p>Configure player: shader (Off/Optimized/Fast/HQ), SVP (desktop), HDR, Visual/Audio presets.</p>
    {:else}
      <p>Optional: add TMDB/MDBList API keys for richer metadata. All settings apply instantly.</p>
    {/if}
    <button on:click={next}>{step === 3 ? 'Finish' : 'Next'}</button>
  </div>
{/if}

<style>
  .wizard { position:fixed; inset:20% 10%; background:#0c0b11; color:#fff; border:2px solid #6a5cff; border-radius:1rem; padding:2rem; z-index:100; display:grid; gap:1rem; place-content:center; text-align:center; }
</style>
