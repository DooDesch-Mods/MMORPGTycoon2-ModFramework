<script setup lang="ts">
import { computed } from 'vue'
import { Content, onContentUpdated, useData, withBase } from 'vitepress'

const { frontmatter, page } = useData()
const home = computed(() => frontmatter.value.layout === 'home')
const currentPath = computed(() => '/' + page.value.relativePath.replace(/\.md$/, '.html'))

const primaryLinks = [
  { text: 'Home', href: '/index.html' },
  { text: 'Quickstart', href: '/quickstart.html' },
  { text: 'API', href: '/reference/index.html', prefix: '/reference/' },
  { text: 'For agents', href: '/agents/start.html', prefix: '/agents/' },
]

const entryLinks = [
  { text: 'Create your first mod', href: '/quickstart.html' },
  { text: 'API reference', href: '/reference/index.html' },
  { text: 'Compatibility', href: '/compatibility.html' },
  { text: 'For coding agents', href: '/agents/start.html' },
]

const guideLinks = [
  { text: 'Install with Vortex', href: '/guides/vortex.html' },
  { text: 'Settings and storage', href: '/guides/config-storage.html' },
  { text: 'Events and input', href: '/guides/events-input.html' },
  { text: 'World actions', href: '/guides/world-actions.html' },
  { text: 'Import GLB scenery', href: '/guides/scenery.html' },
  { text: 'Patch callbacks', href: '/experimental/patches.html' },
  { text: 'Profile a mod', href: '/guides/profiling.html' },
]

const referenceLinks = [
  { text: 'Events', href: '/reference/events.html' },
  { text: 'Types', href: '/reference/types.html' },
  { text: 'Native C++ plugins', href: '/reference/native.html' },
  { text: 'Manifests', href: '/reference/manifests.html' },
  { text: 'Command line', href: '/reference/cli.html' },
  { text: 'Capabilities', href: '/reference/capabilities.html' },
  { text: 'API changes', href: '/reference/changes.html' },
]

const projectLinks = [
  { text: 'Build from source', href: '/contributing/build.html' },
  { text: 'API changes', href: '/reference/changes.html' },
]

const section = computed(() => {
  if (currentPath.value.startsWith('/reference/')) return { title: 'Reference', links: referenceLinks }
  if (currentPath.value.startsWith('/agents/') || currentPath.value.startsWith('/contributing/')) return { title: 'Project', links: projectLinks }
  return { title: 'Guides', links: guideLinks }
})

const sectionLabel = computed(() => {
  if (currentPath.value.startsWith('/reference/')) return 'API reference'
  if (currentPath.value.startsWith('/agents/')) return 'For coding agents'
  if (currentPath.value.startsWith('/contributing/')) return 'Development'
  return 'Documentation'
})

function primaryState(link: typeof primaryLinks[number]) {
  if (link.href === currentPath.value) return 'page'
  if (link.prefix && currentPath.value.startsWith(link.prefix)) return 'location'
  return undefined
}

// Shared search and copy controls also initialize after client-side navigation.
onContentUpdated(() => {
  window.dispatchEvent(new CustomEvent('mt2:content-updated'))
})
</script>

<template>
  <a class="skip" href="#content">Skip to content</a>
  <header class="site-header">
    <div class="header-inner">
      <a class="brand" :href="withBase('/index.html')" aria-label="MT2 Mod Framework home">
        <img :src="withBase('/brand/icon.svg')" width="45" height="45" alt="">
        <span><strong>MT2</strong> Mod Framework</span>
      </a>
      <div class="header-navigation">
        <nav aria-label="Main navigation">
          <a v-for="link in primaryLinks" :key="link.href" :href="withBase(link.href)" :aria-current="primaryState(link)">{{ link.text }}</a>
        </nav>
        <span class="site-version">v0.1.1</span>
      </div>
    </div>
  </header>

  <section class="search-area wrap" aria-label="Documentation search">
    <form id="docs-search" role="search" :action="withBase('/reference/index.html')" method="get">
      <label for="docs-search-input">Search documentation</label>
      <div class="search-controls">
        <input id="docs-search-input" name="q" type="search" placeholder="Try world events or ui.window" autocomplete="off" aria-controls="search-results">
        <button type="submit">Search</button>
      </div>
    </form>
    <div id="search-results" aria-live="polite" aria-label="Search results"></div>
    <p class="search-fallback"><a :href="withBase('/reference/index.html')">Browse the API reference</a></p>
  </section>

  <main v-if="page.isNotFound" id="content" class="not-found wrap" tabindex="-1">
    <p class="eyebrow">Page unavailable</p>
    <h1>This page could not be found.</h1>
    <p>Open the documentation overview to find the current page.</p>
    <a class="button" :href="withBase('/index.html')">Open the overview <span aria-hidden="true">→</span></a>
  </main>
  <main v-else-if="home" id="content" class="home-content" tabindex="-1">
    <Content />
  </main>
  <main v-else id="content" class="docs-shell wrap" tabindex="-1">
    <aside class="docs-sidebar" aria-label="Documentation sections">
      <p class="eyebrow">Start here</p>
      <nav aria-label="Documentation">
        <a v-for="link in entryLinks" :key="link.href" :href="withBase(link.href)" :aria-current="link.href === currentPath ? 'page' : undefined">{{ link.text }}</a>
      </nav>
      <div class="sidebar-section">
        <p class="eyebrow">{{ section.title }}</p>
        <nav :aria-label="section.title">
          <a v-for="link in section.links" :key="link.href" :href="withBase(link.href)" :aria-current="link.href === currentPath ? 'page' : undefined">{{ link.text }}</a>
        </nav>
      </div>
      <div class="sidebar-note">
        <strong>Framework 0.1.1</strong>
        <p>Lua API 1.0.0 - native ABI 1</p>
        <a :href="withBase('/contributing/build.html')">Build from source <span aria-hidden="true">↗</span></a>
      </div>
    </aside>
    <article class="docs-content vp-doc">
      <p class="breadcrumb">{{ sectionLabel }}</p>
      <Content />
    </article>
  </main>

  <footer class="site-footer framework-footer">
    <div>
      <a class="footer-brand" :href="withBase('/index.html')">MT2 Mod Framework</a>
      <p>Independent tools for MMORPG Tycoon 2 modders.</p>
      <p class="footer-support">🛟 <strong>Need help or found a bug?</strong> Get support at <a href="https://support.doodesch.de/modframework">support.doodesch.de/modframework</a>.</p>
    </div>
    <div class="footer-status">
      <p>Lua, C++ and custom scenery.</p>
      <p>Original framework, documentation and icons: MIT.<br>Game imagery belongs to its respective owners.</p>
      <a :href="withBase('/compatibility.html')">Compatibility and current limits <span aria-hidden="true">↗</span></a>
    </div>
  </footer>
</template>
