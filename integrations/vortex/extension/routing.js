// SPDX-License-Identifier: MIT
'use strict';
const path = require('node:path');
const fs = require('node:fs/promises');
const crypto = require('node:crypto');
const GAME_ID = 'mmorpgtycoon2';
const FRAMEWORK_TYPE = 'mt2fw-runtime';
const OWNER = '.mt2fw-vortex-owner.json';
const EXE_SHA256 = 'bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6';
const hash = data => crypto.createHash('sha256').update(data).digest('hex');
function fail(message) { throw new Error('MT2 Framework: ' + message); }
function relative(value) {
  const result = value.replace(/\\/g, '/');
  if (!result || result.startsWith('/') || result.includes(':') || result.split('/').some(p =>
    !p || p === '.' || p === '..' || /[<>"|?*\x00-\x1f]/.test(p) || /[. ]$/.test(p)
    || /^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(p))) fail('Unsafe archive path: ' + value);
  return result;
}
function archiveFiles(files) {
  const seen = new Set();
  return files.filter(file => !/[\\/]$/.test(file)).map(source => {
    const name = relative(source);
    if (seen.has(name.toLowerCase())) fail('Duplicate archive path: ' + name);
    seen.add(name.toLowerCase()); return {source, name};
  });
}
async function readFile(root, name) {
  const parts = relative(name).split('/'); let current = root;
  for (const part of parts) {
    current = path.join(current, part);
    if ((await fs.lstat(current)).isSymbolicLink()) fail('Symbolic link in package: ' + name);
  }
  return fs.readFile(current);
}
async function readJSON(root, name) { return JSON.parse((await readFile(root, name)).toString('utf8')); }
function frameworkName(name) {
  relative(name);
  if (!['WSOCK32.dll', 'MT2Framework.dll'].includes(name) && !name.startsWith('mt2fw/')) fail('File outside framework payload: ' + name);
}
function profilePath(gamePath, profileId) {
  if (!gamePath || !path.isAbsolute(gamePath)) fail('Discover the Steam game first');
  if (!profileId || !/^[A-Za-z0-9_-]+$/.test(profileId)) fail('Select a Vortex profile before deploying');
  // Runtime and mod targets share a volume, so Vortex hardlink deployment works.
  return path.join(path.resolve(gamePath), '.mt2fw-vortex', 'profiles', 'vortex-' + profileId);
}
async function assertGame(gamePath, expected = EXE_SHA256) {
  if (hash(await fs.readFile(path.join(gamePath, 'MT2.exe'))) !== expected) fail('Unsupported Steam game build. Framework files were not deployed.');
}
async function prepareProfile(profile) {
  let cursor = path.resolve(profile);
  while (true) {
    try { if ((await fs.lstat(cursor)).isSymbolicLink()) fail('Framework profile path contains a symbolic link'); }
    catch (error) { if (error.code !== 'ENOENT') throw error; }
    const parent = path.dirname(cursor); if (parent === cursor) break; cursor = parent;
  }
  for (const name of ['mods','data','saves','game-data','logs','diagnostics']) await fs.mkdir(path.join(profile,name),{recursive:true});
}
async function optionalJSON(folder, name) {
  try { return await readJSON(folder,name); } catch(error) { if(error.code === 'ENOENT') return undefined; throw error; }
}
async function verifyOwnership(game, nextFiles = {}, transitionProfile) {
  const owner = await optionalJSON(game, OWNER);
  if (owner && (owner.owner !== 'mt2fw-vortex' || !owner.files)) fail('Invalid Vortex ownership record');
  const names = new Set([...Object.keys(owner?.files || {}), ...Object.keys(nextFiles), 'WSOCK32.dll', 'MT2Framework.dll', 'mt2fw.json']);
  for (const name of names) {
    if (name !== 'mt2fw.json') frameworkName(name);
    let bytes;
    try { bytes = await readFile(game,name); } catch(error) { if (error.code === 'ENOENT') continue; throw error; }
    if (name === 'mt2fw.json') {
      if (!owner) {
        if (!transitionProfile) fail('Existing framework configuration is not Vortex-owned');
        // The helper deliberately retains configuration after removing its binaries.
        // Keep that selection recoverable before Vortex supplies its own profile.
        const backup = path.join(transitionProfile,'backups','vortex-transition');
        await fs.mkdir(backup,{recursive:true});
        const filename = path.join(backup,'configuration-'+hash(bytes)+'.json');
        try { await fs.writeFile(filename,bytes,{flag:'wx'}); }
        catch(error) { if(error.code !== 'EEXIST' || !bytes.equals(await fs.readFile(filename))) throw error; }
      }
    } else if (!owner?.files[name] || hash(bytes) !== owner.files[name]) {
      fail('Refusing to replace an unknown or modified file: ' + name + '. Preserve it and remove a manual framework with its own helper first.');
    }
  }
  return owner;
}
async function installArchive(files, extraction, profile) {
  const rows = archiveFiles(files);
  const packages = rows.filter(row => path.posix.basename(row.name) === 'framework-package.json');
  if (packages.length) {
    if (packages.length !== 1) fail('Archive contains multiple framework packages');
    const root = path.posix.dirname(packages[0].name); const prefix = root === '.' ? '' : root + '/';
    const manifest = await readJSON(extraction,packages[0].name);
    if (!manifest.files || !manifest.files['WSOCK32.dll'] || !manifest.files['MT2Framework.dll']) fail('Framework package misses loader/runtime hashes');
    const instructions = [];
    const expected = new Set([packages[0].name]);
    for (const [destination, checksum] of Object.entries(manifest.files)) {
      frameworkName(destination);
      if (!/^[a-f0-9]{64}$/.test(checksum)) fail('Invalid payload checksum: ' + destination);
      const name = prefix + destination; expected.add(name);
      const row = rows.find(row => row.name === name);
      if (!row || hash(await readFile(extraction,name)) !== checksum) fail('Package checksum differs: ' + destination);
      instructions.push({type:'copy', source:row.source, destination:path.join(...destination.split('/'))});
    }
    if (rows.some(row => !expected.has(row.name))) fail('Unexpected file outside framework manifest');
    instructions.push({type:'generatefile', destination:'mt2fw.json', data:JSON.stringify({profile,enabled:true},null,2)});
    instructions.push({type:'generatefile', destination:OWNER, data:JSON.stringify({owner:'mt2fw-vortex',version:manifest.version,files:manifest.files},null,2)});
    instructions.push({type:'setmodtype',value:FRAMEWORK_TYPE});
    return {instructions};
  }
  const manifests = rows.filter(row => path.posix.basename(row.name) === 'mod.json');
  if (manifests.length !== 1) fail('Expected one framework mod.json. Legacy manifest.json and archive-replacement mods use a different contract.');
  const manifestFile = manifests[0]; const manifest = await readJSON(extraction,manifestFile.name);
  if (typeof manifest.id !== 'string' || !/^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$/.test(manifest.id)) fail('Invalid mod ID');
  const root = path.posix.dirname(manifestFile.name); const prefix = root === '.' ? '' : root + '/';
  const instructions = [];
  for (const row of rows) {
    if (!row.name.startsWith(prefix)) {
      if (/^(readme|license|changelog|credits)(\..*)?$/i.test(path.posix.basename(row.name))) continue;
      fail('File outside mod folder: ' + row.name);
    }
    const destination = row.name.slice(prefix.length); relative(destination);
    instructions.push({type:'copy',source:row.source,destination:path.join(manifest.id,...destination.split('/'))});
  }
  instructions.push({type:'attribute',key:'mt2fwModId',value:manifest.id});
  return {instructions};
}
module.exports = {GAME_ID,FRAMEWORK_TYPE,OWNER,EXE_SHA256,hash,relative,archiveFiles,readJSON,readFile,frameworkName,profilePath,assertGame,prepareProfile,verifyOwnership,installArchive};
