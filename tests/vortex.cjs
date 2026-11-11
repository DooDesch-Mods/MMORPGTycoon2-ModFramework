// SPDX-License-Identifier: MIT
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const Module = require('node:module');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const logs = [];
const load = Module._load;
Module._load = function (request, ...rest) {
  if (request === 'vortex-api') return {log: (...args) => logs.push(args)};
  if (request === 'winapi-bindings') return {RegGetValue: () => ({value: 42})};
  return load.call(this, request, ...rest);
};
const extension = require('../integrations/vortex/extension/index.js');

const root = fs.mkdtempSync(path.join(os.tmpdir(), 'mt2-vortex-'));
test.after(() => fs.rmSync(root, {recursive: true, force: true}));
const game = fs.realpathSync.native(fs.mkdtempSync(path.join(root, 'game-')));
const notices = [];
const state = {
  settings: {gameMode: {discovered: {mmorpgtycoon2: {path: game}}}},
  persistent: {profiles: {p1: {gameId: 'mmorpgtycoon2', modState: {inn: {enabled: true}}}}, mods: {mmorpgtycoon2: {inn: {id: 'inn', attributes: {mt2Classic: true}}}}},
};
const api = {getState: () => state, sendNotification: notice => notices.push(notice.id)};
const steamId = (76561197960265728n + 42n).toString();
const destinations = result => result.instructions.filter(row => row.type !== 'attribute').map(row => row.destination);
const extraction = files => {
  const folder = fs.mkdtempSync(path.join(root, 'archive-'));
  for (const [name, text] of Object.entries(files)) {
    fs.mkdirSync(path.dirname(path.join(folder, name)), {recursive: true});
    fs.writeFileSync(path.join(folder, name), text);
  }
  return folder;
};

test('registers the game and one installer, and warns after a deploy without the Runtime', async () => {
  const calls = [];
  let deployed;
  const context = {
    api: {...api, onAsync: (event, handler) => { calls.push(['event', event]); deployed = handler; }},
    once: callback => callback(),
    registerGame: game => calls.push(['game', game.id]),
    registerInstaller: id => calls.push(['installer', id]),
  };
  assert.equal(extension.default(context), true);
  assert.deepEqual(calls, [['game', 'mmorpgtycoon2'], ['installer', 'mt2-mods'], ['event', 'did-deploy']]);
  notices.length = 0;
  await deployed('p1');
  assert.deepEqual(notices, ['mt2-runtime-missing']);
  fs.writeFileSync(path.join(game, 'mt2fw.json'), '{}');
  await deployed('p1');
  fs.rmSync(path.join(game, 'mt2fw.json'));
  assert.deepEqual(notices, ['mt2-runtime-missing']);
});

test('runtime goes to the game folder with an mt2fw.json for the profile', () => {
  const result = extension.install(api, ['Runtime/', 'Runtime/framework-package.json', 'Runtime/WSOCK32.dll', 'Runtime/mt2fw/core.lua'], root);
  assert.deepEqual(destinations(result), ['framework-package.json', 'WSOCK32.dll', path.join('mt2fw', 'core.lua'), 'mt2fw.json']);
  assert.deepEqual(JSON.parse(result.instructions.at(-1).data), {enabled: true, profile: path.join(game, 'mt2fw-profile')});
});

test('framework mod goes to mods/<id> from a nested folder', () => {
  const folder = extraction({'pack/hello/mod.json': '{"id":"hello.world"}'});
  const result = extension.install(api, ['pack/hello/mod.json', 'pack/hello/main.lua', 'pack/readme.txt'], folder);
  const mod = path.join('mt2fw-profile', 'mods', 'hello.world');
  assert.deepEqual(destinations(result), [path.join(mod, 'mod.json'), path.join(mod, 'main.lua')]);
});

test('framework mod with a bad id or a second mod fails with a log line', () => {
  logs.length = 0;
  assert.throws(() => extension.install(api, ['mod.json'], extraction({'mod.json': '{"id":"../x"}'})), /invalid id/);
  assert.throws(() => extension.install(api, ['a/mod.json', 'b/mod.json'], root), /more than one framework mod/);
  assert.deepEqual(logs.map(row => row[0]), ['error', 'error']);
});

test('classic archive with data folders at the top gets a folder named after the archive', () => {
  const result = extension.install(api, ['buildings/inn.json', 'icons/inn.png', 'readme.txt'], root, path.join(root, 'network_cheat-1-0.zip'));
  const mod = path.join('mt2fw-profile', 'game-data', steamId, 'mod', 'network_cheat-1-0');
  assert.deepEqual(destinations(result), [path.join(mod, 'buildings', 'inn.json'), path.join(mod, 'icons', 'inn.png'), path.join(mod, 'readme.txt')]);
  assert.deepEqual(result.instructions.at(-1), {type: 'attribute', key: 'mt2Classic', value: true});
});

test('classic archive with mod folders at the top keeps them and drops loose files', () => {
  const result = extension.install(api, ['TavernsAsInns/buildings/inn.json', 'readme.txt'], root, path.join(root, 'taverns.zip'));
  assert.deepEqual(destinations(result), [path.join('mt2fw-profile', 'game-data', steamId, 'mod', 'TavernsAsInns', 'buildings', 'inn.json')]);
  assert.throws(() => extension.install(api, ['readme.txt'], root), /no framework runtime, framework mod or classic mod folder/);
});
