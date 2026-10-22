// SPDX-License-Identifier: MIT
'use strict';
const path = require('node:path');
const fs = require('node:fs/promises');
const crypto = require('node:crypto');
const GAME_ID = 'mmorpgtycoon2';
const FRAMEWORK_TYPE = 'mt2fw-runtime';
const SCENERY_TYPE = 'mt2fw-classic-scenery';
const SCENERY_RECORD = '.mt2fw-scenery.json';
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
  return path.join(path.resolve(gamePath), '.mt2fw-vortex', 'profiles', 'vortex-' + profileId);
}
async function assertGame(gamePath, expected = EXE_SHA256) {
  if (hash(await fs.readFile(path.join(gamePath, 'MT2.exe'))) !== expected) fail('Unsupported Steam game build. Framework files were not deployed.');
}
function sceneryPath(profile,account) {
  if (!validSteamAccount(account)) fail('Start Steam and sign in before deploying classic scenery');
  return path.join(profile,'game-data',account,'mod');
}
function validSteamAccount(account) {
  return /^\d{17}$/.test(account || '') && BigInt(account)>76561197960265728n && BigInt(account)<=76561202255233023n;
}
function sceneryName(name) {
  relative(name);
  if (!/^scenery\/(festival|prop|table)\/[^/]+\.vmb$/i.test(name)) fail('Unsupported classic scenery resource: '+name);
}
async function sceneryArchive(rows,extraction) {
  const models=rows.filter(row=>/^(?:.+\/)?scenery\//i.test(row.name));
  if (!models.length) fail('Expected framework mod.json or a classic scenery model pack. Mod Manager manifest.json and archive replacements are unsupported.');
  const roots=new Set(models.map(row=>row.name.slice(0,row.name.toLowerCase().lastIndexOf('scenery/'))));
  if (roots.size!==1) fail('Archive contains multiple classic scenery packs');
  const prefix=[...roots][0];
  if (!prefix) fail('Classic scenery needs a named top-level pack folder');
  const label=prefix.slice(0,-1).split('/').at(-1);
  const id='classic.'+(label.toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'').slice(0,48)||'scenery')+'.'+hash(label.toLowerCase()).slice(0,8);
  const instructions=[],files={};
  for (const row of rows) {
    if (!models.includes(row)) {
      if (/^(readme|license|changelog|credits)(\..*)?$/i.test(path.posix.basename(row.name))) continue;
      fail('Unexpected file in classic scenery archive: '+row.name);
    }
    const name=row.name.slice(prefix.length);sceneryName(name);
    const bytes=await readFile(extraction,row.name);
    if (!['\0\x07ModelV1','\0\x07ModelV2'].some(header=>bytes.subarray(0,9).equals(Buffer.from(header)))) fail('Unsupported VMB header: '+row.name);
    files[name]=hash(bytes);
    instructions.push({type:'copy',source:row.source,destination:path.join(id,...name.split('/'))});
  }
  instructions.push({type:'generatefile',destination:path.join(id,SCENERY_RECORD),data:JSON.stringify({format:1,id,name:label,files},null,2)});
  instructions.push({type:'attribute',key:'mt2fwClassicId',value:id},{type:'setmodtype',value:SCENERY_TYPE});
  return {instructions};
}
async function checkScenery(staging,id) {
  if (!/^classic\.[a-z0-9-]+\.[a-f0-9]{8}$/.test(id || '')) fail('Reinstall this classic scenery pack with Vortex');
  const record=await readJSON(path.join(staging,id),SCENERY_RECORD);
  if (record.format!==1 || record.id!==id || !record.files || !Object.keys(record.files).length) fail('Invalid classic scenery inventory: '+id);
  const expected=new Set([...Object.keys(record.files),SCENERY_RECORD]);
  async function inspect(folder,prefix='') {
    for (const entry of await fs.readdir(folder,{withFileTypes:true})) {
      const name=prefix+entry.name;
      if (entry.isSymbolicLink()) fail('Symbolic link in staged scenery: '+name);
      if (entry.isDirectory()) await inspect(path.join(folder,entry.name),name+'/');
      else if (!entry.isFile() || !expected.has(name)) fail('Unexpected staged scenery file: '+name);
    }
  }
  await inspect(path.join(staging,id));
  for (const [name,expected] of Object.entries(record.files)) {
    sceneryName(name);
    if (hash(await readFile(path.join(staging,id),name))!==expected) fail('Staged scenery checksum differs: '+name);
  }
  return record;
}
function checkSceneryConflicts(records) {
  const names=new Map(),ids=new Set();
  for (const record of records) {
    if (ids.has(record.id)) fail('Two enabled packages use classic scenery ID '+record.id);
    ids.add(record.id);
    for (const name of Object.keys(record.files)) {
      const key=name.toLowerCase();
      if (names.has(key)) fail('Classic scenery resource conflict: '+name+' in '+names.get(key)+' and '+record.name);
      names.set(key,record.name);
    }
  }
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
// Exact release checksum from the standalone Multiplayer Beta 12 package.
// Matching a DLL name or embedded string is not sufficient to replace a loader.
const LEGACY_LOADERS = new Set(['b74272cd89ce6c0d6be2df80e16c665e238458b2b356f8298f15b42cb0623b97']);
async function migrateLegacyLoader(game, profile) {
  if (await optionalJSON(game,OWNER)) return;
  let bytes;
  try { bytes = await readFile(game,'WSOCK32.dll'); }
  catch (error) { if (error.code === 'ENOENT') return; throw error; }
  const checksum = hash(bytes);
  if (!LEGACY_LOADERS.has(checksum)) return;
  const backup = path.join(profile,'backups','standalone-multiplayer',checksum);
  await prepareProfile(profile);
  await fs.mkdir(backup,{recursive:true});
  const filename = path.join(backup,'WSOCK32.dll');
  try { await fs.writeFile(filename,bytes,{flag:'wx'}); }
  catch (error) { if (error.code !== 'EEXIST') throw error; }
  if (!bytes.equals(await fs.readFile(filename))) fail('Standalone loader backup checksum differs');
  await fs.writeFile(path.join(backup,'migration.json'),JSON.stringify({format:1,source:'Multiplayer Beta 12 standalone loader',sha256:checksum,original:path.join(game,'WSOCK32.dll'),backup:filename},null,2));
  if (hash(await readFile(game,'WSOCK32.dll')) !== checksum) fail('Standalone loader changed during backup');
  await fs.unlink(path.join(game,'WSOCK32.dll'));
  return filename;
}
async function verifyOwnership(game, nextFiles = {}, transitionProfile, allowKnownLegacy = false) {
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
      if (allowKnownLegacy && !owner && name === 'WSOCK32.dll' && LEGACY_LOADERS.has(hash(bytes))) continue;
      fail('Refusing to replace an unknown or modified file: ' + name + '. This file is not owned by Vortex or has changed. Remove the previous loader with its own uninstall instructions or helper before deploying. Keep your saves and profiles.');
    }
  }
  return owner;
}
async function releaseRetainedConfiguration(game, profile) {
  if (await optionalJSON(game,OWNER)) return;
  let bytes;
  try { bytes = await readFile(game,'mt2fw.json'); }
  catch (error) { if (error.code === 'ENOENT') return; throw error; }
  const name='backups/vortex-transition/configuration-'+hash(bytes)+'.json';
  if (!bytes.equals(await readFile(profile,name))) fail('Retained configuration backup differs');
  // verifyOwnership has already saved this exact configuration. Free its
  // destination so copy deployment never needs to adopt an unowned file.
  await fs.unlink(path.join(game,'mt2fw.json'));
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
  if (!manifests.length) return sceneryArchive(rows,extraction);
  if (manifests.length !== 1) fail('Expected one framework mod.json. Archive contains multiple mods.');
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
module.exports = {GAME_ID,FRAMEWORK_TYPE,SCENERY_TYPE,SCENERY_RECORD,OWNER,EXE_SHA256,hash,relative,archiveFiles,readJSON,readFile,frameworkName,profilePath,sceneryPath,validSteamAccount,assertGame,prepareProfile,verifyOwnership,migrateLegacyLoader,releaseRetainedConfiguration,installArchive,checkScenery,checkSceneryConflicts};
