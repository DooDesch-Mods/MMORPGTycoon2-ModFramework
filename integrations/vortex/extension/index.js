// SPDX-License-Identifier: MIT
'use strict';
const path = require('node:path');
const fs = require('node:fs');
const {log} = require('vortex-api');

const GAME_ID = 'mmorpgtycoon2';
const STEAM_APP_ID = '486860';
// One framework profile inside the game folder keeps every deployment target on the game's drive,
// which Vortex's hardlink deployment needs.
const PROFILE = 'mt2fw-profile';
// Folders the game reads inside a classic mod folder. An archive that starts with one of them has no mod folder of its own.
// ponytail: names taken from the install notes of the classic mods on Nexus; add one when a classic archive lands a level too deep.
const DATA_DIRS = new Set(['building_definitions', 'building_themes', 'buildings', 'icons', 'materials', 'scenery', 'wall_themes']);

function fail(message, details) {
  log('error', 'MT2: ' + message, details);
  throw new Error('MMORPG Tycoon 2: ' + message);
}

function gamePath(api) {
  const discovered = api.getState().settings.gameMode.discovered[GAME_ID]?.path;
  if (!discovered) fail('the game is not discovered yet');
  // The framework loader rejects a profile path whose letter case differs from the real folder.
  return fs.realpathSync.native(discovered);
}

// The game reads classic mods from <data>/<SteamID64>/mod. With the framework runtime, <data> is the profile's game-data.
function steamId(api) {
  try {
    const user = require('winapi-bindings').RegGetValue('HKEY_CURRENT_USER', 'Software\\Valve\\Steam\\ActiveProcess', 'ActiveUser').value;
    if (user) return (76561197960265728n + BigInt(user)).toString();
    log('info', 'MT2: no Steam user is signed in; looking for one account folder');
  } catch (error) {
    log('warn', 'MT2: reading the Steam user from the registry failed; looking for one account folder', error.message);
  }
  const roots = [path.join(gamePath(api), PROFILE, 'game-data'), path.join(process.env.APPDATA || '', 'VectorStorm', 'MMORPG Tycoon 2')];
  const ids = new Set(roots.flatMap(root => {
    try { return fs.readdirSync(root).filter(name => /^7656\d{13}$/.test(name)); }
    catch (error) { log('info', 'MT2: no account folders in ' + root, error.code); return []; }
  }));
  if (ids.size !== 1) fail('sign in to Steam before installing a classic mod (' + ids.size + ' account folders found)');
  return [...ids][0];
}

// Classic mods sit in the framework profile, so the game reads them only while the deployed Runtime redirects its data folder.
function checkRuntime(api, classicMods) {
  if (fs.existsSync(path.join(gamePath(api), 'mt2fw.json'))) return log('info', 'MT2: the Framework Runtime is deployed for classic mods', classicMods);
  log('warn', 'MT2: classic mods have no effect because the Framework Runtime is not deployed', classicMods);
  api.sendNotification({id: 'mt2-runtime-missing', type: 'warning', title: 'MT2 classic mods have no effect',
    message: 'Enable and deploy the MT2 Mod Framework Runtime. Classic mods load only through it.'});
}

function install(api, files, extraction, archivePath) {
  const rows = files.filter(file => !/[\\/]$/.test(file)).map(file => file.split(/[\\/]/));
  const named = name => rows.filter(row => row.at(-1).toLowerCase() === name).sort((a, b) => a.length - b.length);
  const copy = (root, destination) => rows
    .filter(row => row.length > root.length && root.every((part, index) => row[index] === part))
    .map(row => ({type: 'copy', source: row.join(path.sep), destination: path.join(destination, ...row.slice(root.length))}));

  const [runtime] = named('framework-package.json');
  if (runtime) {
    const profile = path.join(gamePath(api), PROFILE);
    const instructions = copy(runtime.slice(0, -1), '');
    instructions.push({type: 'generatefile', destination: 'mt2fw.json', data: JSON.stringify({enabled: true, profile}, null, 2)});
    log('info', 'MT2: installing the framework runtime into the game folder', {files: instructions.length - 1, profile});
    return {instructions};
  }

  const manifests = named('mod.json');
  if (manifests.length) {
    if (manifests[1]?.length === manifests[0].length) fail('the archive holds more than one framework mod', manifests.map(row => row.join('/')));
    const id = JSON.parse(fs.readFileSync(path.join(extraction, ...manifests[0]), 'utf8')).id;
    if (typeof id !== 'string' || !/^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$/.test(id)) fail('mod.json has an invalid id', id);
    const instructions = copy(manifests[0].slice(0, -1), path.join(PROFILE, 'mods', id));
    log('info', 'MT2: installing framework mod ' + id, {files: instructions.length});
    return {instructions};
  }

  const wrap = rows.some(row => row.length > 1 && DATA_DIRS.has(row[0].toLowerCase()));
  const source = archivePath || extraction;
  const folder = wrap ? [path.basename(source, path.extname(source))] : [];
  const destination = path.join(PROFILE, 'game-data', steamId(api), 'mod', ...folder);
  // Without a wrapping folder, every top-level folder is a mod folder; loose top-level files are readmes.
  const instructions = wrap ? copy([], destination) : copy([], destination).filter(row => row.source.includes(path.sep));
  if (!instructions.length) fail('the archive has no framework runtime, framework mod or classic mod folder');
  log('info', 'MT2: installing a classic mod into ' + destination, {files: instructions.length, wrapped: wrap});
  checkRuntime(api, [path.basename(source)]);
  instructions.push({type: 'attribute', key: 'mt2Classic', value: true});
  return {instructions};
}

function main(context) {
  context.registerGame({
    id: GAME_ID,
    name: 'MMORPG Tycoon 2',
    mergeMods: true,
    queryArgs: {steam: [{id: STEAM_APP_ID}]},
    queryModPath: () => '.',
    logo: 'gameart.png',
    executable: () => 'MT2.exe',
    requiredFiles: ['MT2.exe'],
    requiresLauncher: () => Promise.resolve({launcher: 'steam'}),
    environment: {SteamAppId: STEAM_APP_ID},
    details: {steamAppId: Number(STEAM_APP_ID), nexusPageId: GAME_ID},
  });
  context.registerInstaller('mt2-mods', 25, (files, gameId) => Promise.resolve({supported: gameId === GAME_ID, requiredFiles: []}),
    async (files, extraction, gameId, progress, choices, unattended, archivePath) => install(context.api, files, extraction, archivePath));
  context.once(() => context.api.onAsync('did-deploy', async profileId => {
    const state = context.api.getState();
    const profile = state.persistent.profiles[profileId];
    if (profile?.gameId !== GAME_ID) return log('debug', 'MT2: deployment of another game', profileId);
    const classic = Object.values(state.persistent.mods[GAME_ID] ?? {})
      .filter(mod => mod.attributes?.mt2Classic && profile.modState?.[mod.id]?.enabled).map(mod => mod.id);
    if (!classic.length) return log('debug', 'MT2: deployed without classic mods');
    checkRuntime(context.api, classic);
  }));
  log('info', 'MT2: game extension registered');
  return true;
}

module.exports = {default: main, install};
