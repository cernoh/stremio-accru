<script lang="ts">
  import { onMount } from 'svelte';

  let hidden = true;
  let oled = false;

  onMount(() => {
    oled = localStorage.getItem('stremio-oled-theme-enabled') === 'true';
    document.documentElement.dataset.oled = String(oled);
    const obs = new MutationObserver(() => {
      requestAnimationFrame(() => {
        const el = document.querySelector('[data-oled]');
        if (el) el.setAttribute('data-oled', String(oled));
      });
    });
    obs.observe(document.body, { childList: true, subtree: true });
    return () => obs.disconnect();
  });

  function toggleOled(): void {
    oled = !oled;
    localStorage.setItem('stremio-oled-theme-enabled', String(oled));
    document.documentElement.dataset.oled = String(oled);
    document.documentElement.style.setProperty(
      '--primary-background-color',
      oled ? '#000000' : '#0c0b11',
    );
    document.documentElement.style.setProperty(
      '--secondary-background-color',
      oled ? '#000000' : '#1a173e',
    );
  }
</script>

<nav
  class="hidden-nav"
  class:hidden
  on:mouseenter={() => (hidden = false)}
  on:mouseleave={() => (hidden = true)}
  aria-label="Hidden navigation"
>
  <div
    class="nav-content"
    style:opacity={hidden ? 0 : 1}
    style:transition="opacity 200ms"
  >
    <a href="#/">Discover</a> · <a href="#/board">Board</a> · <a href="#/library">Library</a>
    <button on:click={toggleOled}>OLED: {oled ? 'ON' : 'OFF'}</button>
    <span style="opacity:.6">Hover to show • Drawer on mobile • Clock/ETA • Gamepad guard</span>
  </div>
</nav>

<style>
  .hidden-nav {
    position: sticky;
    top: 0;
    background: #0c0b11;
    color: #fff;
    padding: 0.5rem 1rem;
    border-bottom: 1px solid #222;
  }
  .hidden-nav.hidden { opacity: 0.9; }
  .hidden-nav:not(.hidden) { opacity: 1; }
  :global([data-oled="true"]) {
    --primary-background-color: #000;
    --secondary-background-color: #000;
    background: #000 !important;
  }
</style>
