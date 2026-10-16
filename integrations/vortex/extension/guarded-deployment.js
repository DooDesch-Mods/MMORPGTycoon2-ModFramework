// SPDX-License-Identifier: MIT
'use strict';
const ID = 'mt2fw-hardlink';
function createGuardedDeployment(getBase,preflight,purgeGuard,removeGuard,changesGuard=purgeGuard) {
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
      return base().isSupported(state,gameId,typeId);
    },
    async prepare(...args) { await preflight();return base().prepare(...args); },
    async externalChanges(...args) { await changesGuard();return base().externalChanges(...args); },
    async prePurge(...args) { await purgeGuard();return base().prePurge(...args); },
    async purge(...args) { await purgeGuard();return base().purge(...args); },
    async deactivate(...args) { await removeGuard();return base().deactivate(...args); },
  };
  return new Proxy(guarded,{get(target,property) {
    if (property in target) return target[property];
    const owner = base(),value = owner[property];
    return typeof value === 'function' ? value.bind(owner) : value;
  }});
}
module.exports = {ID,createGuardedDeployment};
