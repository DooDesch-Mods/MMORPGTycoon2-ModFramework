/* Shared plain-browser controls also work when documentation is opened from a file. */
(() => {
  const source = document.currentScript;
  const root = new URL('.', source.src);
  function search(query) {
    const output = document.getElementById('search-results');
    if (!output) return;
    output.replaceChildren();
    const terms = query.trim().toLowerCase().split(/\s+/).filter(Boolean);
    if (!terms.length) return;
    const rows = (window.MT2FW_SEARCH_INDEX || []).map(row => {
      const text = (row.title + ' ' + row.text).toLowerCase();
      return { row, score: terms.every(term => text.includes(term)) ? terms.reduce((n, term) => n + (row.title.toLowerCase().includes(term) ? 10 : 1), 0) : 0 };
    }).filter(row => row.score).sort((a, b) => b.score - a.score).slice(0, 8);
    const status = document.createElement('p');
    status.textContent = rows.length ? `${rows.length} matching pages` : 'No matching pages. Try a function or capability name.';
    output.append(status);
    const list = document.createElement('ul');
    for (const { row } of rows) {
      const item = document.createElement('li');
      const link = document.createElement('a');
      link.href = new URL(row.href, root).href;
      link.textContent = row.title;
      item.append(link); list.append(item);
    }
    output.append(list);
  }
  document.addEventListener('submit', event => {
    if (event.target.id !== 'docs-search') return;
    event.preventDefault(); search(event.target.elements.q.value);
  });
  document.addEventListener('input', event => {
    if (event.target.id === 'docs-search-input') search(event.target.value);
  });
  document.addEventListener('click', async event => {
    const button = event.target.closest('button.copy');
    if (!button) return;
    const code = button.parentElement.querySelector('pre code');
    if (!code) return;
    try {
      await navigator.clipboard.writeText(code.innerText);
      button.classList.add('copied');button.setAttribute('aria-label', 'Code copied');
      setTimeout(() => { button.classList.remove('copied');button.setAttribute('aria-label', 'Copy code'); }, 1800);
    } catch (error) {
      console.error('Copy code failed:', error);
      button.setAttribute('aria-label', 'Copy failed; select the code and copy manually');
      button.title = 'Copy failed; select the code and copy manually';
    }
  });
  function initialize() {
    for (const button of document.querySelectorAll('button.copy')) button.setAttribute('aria-label', 'Copy code');
    const query = new URLSearchParams(location.search).get('q');
    if (query) { const input = document.getElementById('docs-search-input');if (input) input.value = query;search(query); }
  }
  document.addEventListener('DOMContentLoaded', initialize);
  window.addEventListener('mt2:content-updated', initialize);
  if (document.readyState !== 'loading') initialize();
})();
