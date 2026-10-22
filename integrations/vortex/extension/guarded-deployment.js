// SPDX-License-Identifier: MIT
'use strict';
const ID = 'mt2fw-hardlink';
function checkActivator(_state,action) {
  const {gameId,activatorId} = action.payload;
  // Vortex clears an unavailable method before the user repairs the staging path.
  return gameId === 'mmorpgtycoon2' && activatorId != null && activatorId !== '' && activatorId !== ID
    ? 'MT2 Framework requires protected hardlink deployment' : undefined;
}
function createGuardedDeployment(getBase,preflight,purgeGuard,removeGuard,changesGuard=purgeGuard,paths) {
  const base = () => {
    const value = getBase();
    if (!value) throw new Error('MT2 Framework: Vortex hardlink deployment is unavailable');
    return value;
  };
  // Event notifications do not propagate rejection to Vortex's deployment pipeline.
  // Guard the awaited deployment methods before the built-in implementation writes files.
  const guarded = {
    id:ID,name:'MT2 protected hardlink deployment',priority:0,compatible:['hardlink_activator'],
    isSupported(state,gameId,typeId) {
      if (gameId !== 'mmorpgtycoon2') return {description:()=> 'MT2 Framework only'};
      const problem = base().isSupported(state,gameId,typeId);
      if (problem && paths) {
        const {staging,game} = paths(state);
        if (staging && game) {
          const path = require('node:path').win32;
          const volume = path.parse(game).root;
          if (volume && volume.toLowerCase() !== path.parse(staging).root.toLowerCase()) {
            return {...problem,
              description:()=> `MT2 staging folder is on ${path.parse(staging).root}; the game is on ${volume}.`,
              solution:()=> `Open Settings > Mods and move the Mod Staging Folder to ${volume}, outside the game folder. Use Suggest or choose a folder there, then Apply.`,
            };
          }
        }
      }
      return problem;
    },
    async prepare(...args) { await preflight();return base().prepare(...args); },
    async externalChanges(...args) { await changesGuard();return base().externalChanges(...args); },
    async prePurge(...args) { if (await purgeGuard() === false) return;return base().prePurge(...args); },
    async purge(...args) { if (await purgeGuard() === false) return;return base().purge(...args); },
    async deactivate(...args) { await removeGuard();return base().deactivate(...args); },
  };
  return new Proxy(guarded,{get(target,property) {
    if (property in target) return target[property];
    const owner = base(),value = owner[property];
    return typeof value === 'function' ? value.bind(owner) : value;
  }});
}
module.exports = {ID,createGuardedDeployment,checkActivator};
