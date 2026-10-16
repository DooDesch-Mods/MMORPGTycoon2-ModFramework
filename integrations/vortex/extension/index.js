// SPDX-License-Identifier: MIT
'use strict';
const path = require('node:path');
const fs = require('node:fs/promises');
const syncFS = require('node:fs');
const {execFile} = require('node:child_process');
const {promisify} = require('node:util');
const {util,selectors,actions} = require('vortex-api');
const routing = require('./routing');
const deployment = require('./guarded-deployment');
const run = promisify(execFile);
const {GAME_ID,FRAMEWORK_TYPE} = routing;
const error = message => { throw new Error('MT2 Framework: ' + message); };
function profileId(api) {
  const state = api.getState();
  for (const id of [state.settings.profiles.nextProfileId,state.settings.profiles.activeProfileId]) {
    if (state.persistent.profiles[id]?.gameId === GAME_ID) return id;
  }
  error('Select an MT2 Vortex profile before deployment');
}
function modPath(api,game) {
  const profile = routing.profilePath(game,profileId(api));
  let cursor = profile;
  while (true) {
    try { if (syncFS.lstatSync(cursor).isSymbolicLink()) error('Framework profile path contains a symbolic link'); }
    catch(cause) { if (cause.code !== 'ENOENT') throw cause; }
    const parent = path.dirname(cursor); if (parent === cursor) break; cursor = parent;
  }
  // Vortex checks deployment availability before it emits will-deploy.
  const result = path.join(profile,'mods');syncFS.mkdirSync(result,{recursive:true});return result;
}
function gamePath(api) {
  const result = api.getState().settings.gameMode.discovered[GAME_ID]?.path;
  if (!result) error('Discover the Steam game first'); return result;
}
async function requireClosed(game) {
  const command = 'Get-CimInstance Win32_Process -Filter "Name=\'MT2.exe\'" | Select-Object ExecutablePath,ProcessId | ConvertTo-Json -Compress';
  const {stdout} = await run('powershell.exe',['-NoProfile','-NonInteractive','-Command',command],{windowsHide:true,encoding:'utf8'});
  if (!stdout.trim()) return;
  const rows = JSON.parse(stdout);
  for (const row of Array.isArray(rows) ? rows : [rows]) {
    if (!row.ExecutablePath || path.resolve(row.ExecutablePath).toLowerCase() === path.join(game,'MT2.exe').toLowerCase()) error('Close MT2 before deployment or purge');
  }
}
async function willDeploy(api, id) {
  const state = api.getState(); const profile = state.persistent.profiles[id];
  if (!profile || profile.gameId !== GAME_ID) return;
  const game = gamePath(api); await requireClosed(game); await routing.assertGame(game);
  const mods = state.persistent.mods[GAME_ID] || {};
  const enabled = Object.entries(mods).filter(([key]) => profile.modState[key]?.enabled);
  const runtimes = enabled.filter(([,mod]) => mod.type === FRAMEWORK_TYPE);
  if (runtimes.length > 1) error('Enable exactly one framework version');
  const content = enabled.filter(([,mod]) => mod.type !== FRAMEWORK_TYPE);
  if (content.length && runtimes.length !== 1) error('Enable the MT2 Framework package before deploying its mods');
  const staging = selectors.installPathForGame(state,GAME_ID);
  const target = routing.profilePath(game,id); await routing.prepareProfile(target);
  if (runtimes.length) {
    const [,runtime] = runtimes[0]; const directory = path.join(staging,runtime.installationPath);
    const owner = await routing.readJSON(directory,routing.OWNER);
    await routing.verifyOwnership(game,owner.files,target);
    for (const [name,expected] of Object.entries(owner.files)) {
      routing.frameworkName(name);
      if (routing.hash(await routing.readFile(directory,name)) !== expected) error('Staged framework checksum differs: ' + name);
    }
    const seen = new Set();
    for (const [,mod] of content) {
      const modId = mod.attributes?.mt2fwModId;
      if (!modId) error('Reinstall this mod with the MT2 Framework installer');
      if (seen.has(modId)) error('Two enabled packages use mod ID ' + modId); seen.add(modId);
      const folder = path.join(staging,mod.installationPath,modId);
      try { await run(path.join(directory,'mt2fw','mt2fw.exe'),['check',folder],{windowsHide:true,encoding:'utf8'}); }
      catch (cause) { error('Mod '+modId+' failed the shared CLI validation: '+(cause.stderr || cause.message)); }
    }
    // Vortex tracks this generated file. Update the staging copy before it deploys the selected profile.
    await fs.writeFile(path.join(directory,'mt2fw.json'),JSON.stringify({profile:target,enabled:true},null,2));
  } else {
    await routing.verifyOwnership(game,{},target);
  }
}
async function willPurge(api,id) {
  const profile = api.getState().persistent.profiles[id];
  if (profile?.gameId !== GAME_ID) return;
  const game = gamePath(api); await requireClosed(game);
  await routing.verifyOwnership(game,{},routing.profilePath(game,id));
}
function main(context) {
  const api = context.api;
  context.registerDeploymentMethod(deployment.createGuardedDeployment(
    ()=>util.getActivator('hardlink_activator'),
    ()=>willDeploy(api,profileId(api)),
    ()=>willPurge(api,profileId(api)),
    ()=>requireClosed(gamePath(api))));
  context.registerActionCheck('SET_ACTIVATOR',(_state,action)=>
    action.payload.gameId === GAME_ID && action.payload.activatorId !== deployment.ID
      ? 'MT2 Framework requires protected hardlink deployment' : undefined);
  context.registerGame({
    id:GAME_ID,name:'MMORPG Tycoon 2',mergeMods:true,
    queryPath:()=>util.GameStoreHelper.findByAppId(['486860'],'steam').then(game=>game.gamePath),
    queryModPath:game=>modPath(api,game),
    executable:()=> 'MT2.exe',requiredFiles:['MT2.exe'],logo:'gameart.png',
    setup:async discovery=>{await routing.prepareProfile(routing.profilePath(discovery.path,profileId(api)));api.store.dispatch(actions.setActivator(GAME_ID,deployment.ID));},
    requiresLauncher:()=>Promise.resolve({launcher:'steam'}),
    environment:{SteamAppId:'486860'},
    details:{steamAppId:486860,nexusPageId:GAME_ID,supportsSymlinks:false},
    supportedTools:[{id:'mt2fw-helper',name:'MT2 Framework Helper',executable:()=>path.join('mt2fw','MT2FW-Helper.exe'),requiredFiles:[path.join('mt2fw','MT2FW-Helper.exe')],relative:true}],
  });
  context.registerModType(FRAMEWORK_TYPE,25,id=>id===GAME_ID,()=>api.getState().settings.gameMode.discovered[GAME_ID]?.path,
    instructions=>Promise.resolve(instructions.some(row=>row.type==='generatefile' && row.destination===routing.OWNER)),{name:'MT2 Framework runtime'});
  context.registerInstaller('mt2fw-packages',25,(_files,id)=>Promise.resolve({supported:id===GAME_ID,requiredFiles:[]}),
    (files,extraction)=>routing.installArchive(files,extraction,routing.profilePath(gamePath(api),profileId(api))));
  return true;
}
module.exports = {default:main,willDeploy,willPurge,requireClosed};
