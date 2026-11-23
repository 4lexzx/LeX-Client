import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwind from '@tailwindcss/vite';
import { fileURLToPath } from 'node:url';

const pkg = (p: string) => fileURLToPath(new URL(`../../packages/${p}/src/index${p === 'ui-components' ? '.tsx' : '.ts'}`, import.meta.url));

export default defineConfig({
  base: './',
  plugins: [react(), tailwind()],
  server: {
    host: '127.0.0.1',
    port: 5173,
    strictPort: true,
  },
  build: {
    outDir: 'dist/renderer',
    emptyOutDir: true,
  },
  resolve: {
    alias: {
      '@lex/shared': pkg('shared'),
      '@lex/ui-components': pkg('ui-components'),
      '@lex/module-sdk': pkg('module-sdk'),
      '@lex/compatibility': pkg('compatibility'),
      '@lex/cosmetics': fileURLToPath(new URL('../../cosmetics/src/index.ts', import.meta.url)),
    },
  },
  define: {
    'process.env.NODE_ENV': JSON.stringify(process.env.NODE_ENV ?? 'development'),
  },
});
