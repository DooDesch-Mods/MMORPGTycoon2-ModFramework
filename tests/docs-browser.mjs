import { chromium, expect } from '@playwright/test'
import { createServer } from 'node:http'
import { readFile, stat, writeFile, mkdir } from 'node:fs/promises'
import { dirname, extname, isAbsolute, relative, resolve } from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'
import { performance } from 'node:perf_hooks'

const repository = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const argumentsList = process.argv.slice(2)
function argument(name, fallback) {
  const index = argumentsList.indexOf(name)
  if (index < 0) return fallback
  if (!argumentsList[index + 1] || argumentsList[index + 1].startsWith('--')) throw new Error(`Missing value for ${name}`)
  return argumentsList[index + 1]
}

const siteDirectory = resolve(argument('--site', resolve(repository, 'docs/.vitepress/dist')))
const offlineArgument = argument('--offline', process.env.DOCS_OFFLINE_DIR)
if (!offlineArgument) throw new Error('Provide --offline with the generated offline documentation directory.')
const offlineDirectory = resolve(offlineArgument)
const reportPath = resolve(argument('--report', resolve(repository, 'artifacts/docs-browser.json')))
const captureArgument = argument('--captures', undefined)
const captureDirectory = captureArgument ? resolve(captureArgument) : undefined
const executablePath = process.env.CHROMIUM_EXECUTABLE || chromium.executablePath()
const pages = ['index.html', 'quickstart.html', 'reference/index.html', 'reference/functions/ui-window.html', 'reference/native.html', 'guides/scenery.html', 'agents/start.html', 'experimental/patches.html', 'reference/functions/patches-add.html']
const mimeTypes = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png', '.jpg': 'image/jpeg', '.woff2': 'font/woff2' }
const started = performance.now()
const report = { status: 'running', checks: [], errors: [], blockedRequests: [], navigationWait: 'Idle callback, animation frame and network idle before leaving a document.' }

if (captureDirectory) await mkdir(captureDirectory, { recursive: true })

for (const root of [siteDirectory, offlineDirectory]) {
  const entry = resolve(root, 'index.html')
  if (!(await stat(entry)).isFile()) throw new Error(`Documentation entry is not a file: ${entry}`)
}

const server = createServer(async (request, response) => {
  try {
    const requestUrl = new URL(request.url || '/', 'http://127.0.0.1')
    const pathname = decodeURIComponent(requestUrl.pathname)
    const file = resolve(siteDirectory, '.' + (pathname.endsWith('/') ? pathname + 'index.html' : pathname))
    const within = relative(siteDirectory, file)
    if (within.startsWith('..') || isAbsolute(within)) {
      response.writeHead(403).end('Outside documentation directory')
      return
    }
    const body = await readFile(file)
    response.writeHead(200, { 'Content-Type': mimeTypes[extname(file)] || 'application/octet-stream', 'Cache-Control': 'no-store' }).end(body)
  } catch (error) {
    const status = error.code === 'ENOENT' ? 404 : 500
    report.errors.push({ operation: 'serve', target: request.url, error: error.message })
    response.writeHead(status).end(status === 404 ? 'File not found' : 'Unable to read documentation file')
  }
})

await new Promise((ready, reject) => {
  server.once('error', reject)
  server.listen(0, '127.0.0.1', ready)
})
const origin = `http://127.0.0.1:${server.address().port}`
let browser
let context
let page
let originalClipboard
let mode = 'site'

function urlFor(file) {
  return mode === 'offline' ? pathToFileURL(resolve(offlineDirectory, file)).href : `${origin}/${file}`
}

async function open(file) {
  await settlePage()
  await page.goto(urlFor(file), { waitUntil: 'load' })
  await expect(page.locator('main#content h1')).toBeVisible()
  await settlePage()
}

async function settlePage() {
  if (page.url() === 'about:blank') return
  // VitePress schedules visible-link prefetch from requestIdleCallback.
  // Let that queue reach the network before another document cancels it.
  await page.evaluate(() => new Promise(done => {
    requestIdleCallback(() => requestAnimationFrame(() => requestAnimationFrame(done)))
  }))
  await page.waitForLoadState('networkidle')
}

async function switchMode(nextMode) {
  await settlePage()
  if (mode === nextMode) return
  // Detach the previous document while its file or HTTP policy still applies.
  // Clipboard restoration also crosses this boundary after the offline checks.
  await page.goto('about:blank', { waitUntil: 'load' })
  await page.waitForLoadState('networkidle')
  mode = nextMode
}

try {
  browser = await chromium.launch({ executablePath, headless: true })
  context = await browser.newContext({ permissions: ['clipboard-read', 'clipboard-write'] })
  page = await context.newPage()
  page.on('pageerror', error => report.errors.push({ operation: 'browser', mode, url: page.url(), error: error.message }))
  page.on('console', message => {
    if (message.type() === 'error') report.errors.push({ operation: 'console', mode, url: page.url(), error: message.text() })
  })
  page.on('requestfailed', request => report.errors.push({ operation: 'request', mode, url: request.url(), error: request.failure()?.errorText }))
  await context.route('**/*', route => {
    const url = route.request().url()
    const permitted = mode === 'offline' ? url.startsWith('file:') || url.startsWith('data:') : url.startsWith(origin + '/') || url.startsWith('data:')
    if (permitted) return route.continue()
    report.blockedRequests.push({ mode, url })
    return route.abort('blockedbyclient')
  })
  await open('index.html')
  originalClipboard = await page.evaluate(() => navigator.clipboard.readText())

  for (const outputMode of ['site', 'offline']) {
    // A skip-link scroll can reveal new prefetch links in the previous output.
    // Drain those under its existing network policy before switching modes.
    await switchMode(outputMode)
    for (const width of [1440, 390]) {
      await page.setViewportSize({ width, height: width === 390 ? 844 : 1000 })
      for (const file of pages) {
        await open(file)
        const result = await page.evaluate(async () => {
          const imageErrors = []
          for (const image of document.images) {
            try { await image.decode() }
            catch (error) { imageErrors.push({ src: image.src, error: error.message }) }
          }
          return {
            viewport: innerWidth,
            documentWidth: document.documentElement.scrollWidth,
            imageErrors,
            headingCount: document.querySelectorAll('main h1').length,
            moduleScripts: document.querySelectorAll('script[type=module]').length,
          }
        })
        expect(result.documentWidth, `${mode} ${file}: page overflow`).toBeLessThanOrEqual(width + 1)
        expect(result.imageErrors, `${mode} ${file}: images`).toEqual([])
        expect(result.headingCount, `${mode} ${file}: heading hierarchy`).toBe(1)
        if (mode === 'offline') expect(result.moduleScripts, `${file}: offline module dependency`).toBe(0)
        if (captureDirectory && mode === 'site') {
          result.capture = resolve(captureDirectory, file.replace(/\.html$/, '').replaceAll('/', '-') + `-${width}.png`)
          await page.screenshot({ path: result.capture, fullPage: true })
        }
        report.checks.push({ mode, file, ...result })
      }
    }

    await page.setViewportSize({ width: 390, height: 844 })
    await open('index.html')
    await page.getByRole('navigation', { name: 'Main navigation' }).getByRole('link', { name: 'Quickstart', exact: true }).click()
    await expect(page).toHaveURL(/\/quickstart\.html$/)
    await expect(page.locator('main h1')).toBeVisible()
    await settlePage()
    await page.getByRole('navigation', { name: 'Main navigation' }).getByRole('link', { name: 'API', exact: true }).click()
    await expect(page).toHaveURL(/\/reference\/(index\.html)?$/)
    await settlePage()
    await page.locator('#docs-search input[name=q]').fill('ui.window')
    await page.locator('#docs-search button[type=submit]').click()
    const resultLink = page.locator('#search-results a').filter({ hasText: 'ui.window' }).first()
    await expect(resultLink).toBeVisible()
    await resultLink.click()
    await expect(page).toHaveURL(/\/reference\/functions\/ui-window\.html$/)
    await expect(page.locator('main h1')).toContainText('ui.window')
    await settlePage()

    const codeBlock = page.locator('main div[class*="language-"]').filter({ has: page.locator('pre code') }).first()
    const expectedCode = await codeBlock.locator('pre code').innerText()
    await codeBlock.locator('button.copy').click()
    await expect.poll(() => page.evaluate(() => navigator.clipboard.readText()), { message: `${mode}: copied code must reach the clipboard` }).toBe(expectedCode)

    await open('index.html')
    await page.keyboard.press('Tab')
    await expect(page.getByRole('link', { name: 'Skip to content', exact: true })).toBeFocused()
    const focus = await page.evaluate(() => ({ top: document.activeElement.getBoundingClientRect().top, outline: getComputedStyle(document.activeElement).outlineStyle }))
    expect(focus.top).toBeGreaterThanOrEqual(0)
    expect(focus.outline).not.toBe('none')
    await page.keyboard.press('Enter')
    await expect(page).toHaveURL(/#content$/)
    report.checks.push({ mode, interactions: ['main navigation', 'search after navigation', 'search result navigation', 'actual code clipboard value', 'keyboard skip link'] })
  }
  expect(report.errors, 'Browser and serving errors').toEqual([])
  expect(report.blockedRequests, 'Documentation must not request external resources').toEqual([])
  report.status = 'passed'
} catch (error) {
  report.status = 'failed'
  report.failure = { mode, url: page?.url(), message: error.message, stack: error.stack }
  process.exitCode = 1
} finally {
  if (page && originalClipboard !== undefined) {
    try {
      await switchMode('site')
      await page.goto(`${origin}/index.html`, { waitUntil: 'load' })
      await page.evaluate(text => navigator.clipboard.writeText(text), originalClipboard)
      await settlePage()
    } catch (error) {
      report.status = 'failed'
      report.errors.push({ operation: 'restore clipboard', error: error.message })
      process.exitCode = 1
    }
  }
  if (browser) await browser.close()
  await new Promise((done, reject) => server.close(error => error ? reject(error) : done()))
  if (report.errors.length || report.blockedRequests.length) {
    report.status = 'failed'
    process.exitCode = 1
  }
  report.elapsedSeconds = Number(((performance.now() - started) / 1000).toFixed(2))
  await mkdir(dirname(reportPath), { recursive: true })
  await writeFile(reportPath, JSON.stringify(report, null, 2) + '\n')
  console.log(JSON.stringify({ status: report.status, checks: report.checks.length, errors: report.errors.length, elapsedSeconds: report.elapsedSeconds, reportPath, failure: report.failure?.message }))
}
