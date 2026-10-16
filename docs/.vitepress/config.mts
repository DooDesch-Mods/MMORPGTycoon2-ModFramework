import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'MT2 Mod Framework',
  description: 'Build mods for MMORPG Tycoon 2 with Lua scripts, C++ plugins and GLB scenery.',
  lang: 'en-US',
  base: '/',
  cleanUrls: false,
  ignoreDeadLinks: false,
  appearance: 'force-dark',
  lastUpdated: false,
  head: [
    ['meta', { name: 'theme-color', content: '#0b1723' }],
    ['link', { rel: 'icon', type: 'image/svg+xml', href: '/brand/icon-small.svg' }],
    ['script', { src: '/search-index.js', defer: '' }],
    ['script', { src: '/docs-ui.js', defer: '' }],
  ],
  markdown: {
    theme: 'github-dark',
    lineNumbers: false,
    externalLinks: { target: '_blank', rel: 'noopener noreferrer' },
  },
})
