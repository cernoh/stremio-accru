import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
  plugins: [sveltekit()],
  clearScreen: false,
  server: { strictPort: true },
  envPrefix: ['VITE_', 'TAURI_'],
  build: { target: process.env.TAURI_PLATFORM === 'windows' ? 'chrome105' : 'safari13' }
});
