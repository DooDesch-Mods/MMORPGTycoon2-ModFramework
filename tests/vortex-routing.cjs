'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const r = require('../integrations/vortex/extension/routing');
const {createGuardedDeployment} = require('../integrations/vortex/extension/guarded-deployment');
test('awaited guards stop deployment and purge before file operations',async()=>{
  const calls=[];
  const base={isSupported:()=>undefined,prepare(){calls.push('prepare');},externalChanges(){calls.push('externalChanges');},prePurge(){calls.push('prePurge');},purge(){calls.push('purge');},deactivate(){calls.push('deactivate');},finalize(){assert.equal(this,base);calls.push('finalize');}};
  const reject=async()=>{throw new Error('foreign loader');};
  const blocked=createGuardedDeployment(()=>base,reject,reject,reject);
  for(const method of ['prepare','externalChanges','prePurge','purge','deactivate'])await assert.rejects(blocked[method](),/foreign loader/);
  assert.deepEqual(calls,[]);
  const allowed=createGuardedDeployment(()=>base,async()=>{},async()=>{},async()=>{});
  assert.ok(allowed.isSupported({},'other-game'));
  assert.equal(allowed.isSupported({},r.GAME_ID),undefined);
  for(const method of ['prepare','externalChanges','prePurge','purge','deactivate','finalize'])await allowed[method]();
  assert.deepEqual(calls,['prepare','externalChanges','prePurge','purge','deactivate','finalize']);
});
async function fixture() { return fs.mkdtemp(path.join(os.tmpdir(),'mt2fw-vortex-')); }
async function write(root,name,value) { const dest=path.join(root,name); await fs.mkdir(path.dirname(dest),{recursive:true});await fs.writeFile(dest,typeof value==='object'&&!Buffer.isBuffer(value)?JSON.stringify(value):value); }
test('reject path escapes, Windows aliases and case collisions',()=>{
  for(const name of ['../MT2.exe','C:/x','/root','a/../x','a//b','NUL.txt','a./x','a /x','x:stream']) assert.throws(()=>r.relative(name));
  assert.throws(()=>r.archiveFiles(['mod.json','MOD.JSON']));
});
test('mod folder routes by manifest ID and retains editor SDK',async()=>{
  const root=await fixture();await write(root,'wrapped/mod.json',{id:'test.hello'});await write(root,'wrapped/main.lua','return {}');await write(root,'wrapped/.mt2fw/api.json',{});
  const result=await r.installArchive(['wrapped/mod.json','wrapped/main.lua','wrapped/.mt2fw/api.json'],root,'profile');
  assert.deepEqual(result.instructions.filter(i=>i.type==='copy').map(i=>i.destination),['test.hello/mod.json','test.hello/main.lua','test.hello/.mt2fw/api.json'].map(p=>path.join(...p.split('/'))));
  assert.equal(result.instructions.at(-1).value,'test.hello');
});
test('reject ambiguous, legacy and stray executable archives',async()=>{
  const root=await fixture();await write(root,'mod.json',{id:'test.hello'});
  await assert.rejects(r.installArchive(['legacy/manifest.json'],root,'profile'),/mod.json/);
  await assert.rejects(r.installArchive(['one/mod.json','two/mod.json'],root,'profile'),/one framework/);
  await write(root,'wrapped/mod.json',{id:'test.hello'});
  await assert.rejects(r.installArchive(['wrapped/mod.json','MT2.exe'],root,'profile'),/outside mod/);
});
test('framework checks all hashes, generates owned configuration, rejects extra payload',async()=>{
  const root=await fixture();const files={'WSOCK32.dll':r.hash('loader'),'MT2Framework.dll':r.hash('runtime')};
  await write(root,'framework-package.json',{version:'0.1.0',files});await write(root,'WSOCK32.dll','loader');await write(root,'MT2Framework.dll','runtime');
  const list=['framework-package.json',...Object.keys(files)];
  const result=await r.installArchive(list,root,'private-profile');
  assert.equal(result.instructions.at(-1).value,r.FRAMEWORK_TYPE);
  assert.equal(JSON.parse(result.instructions.find(i=>i.destination==='mt2fw.json').data).profile,'private-profile');
  await assert.rejects(r.installArchive([...list,'MT2.exe'],root,'p'),/Unexpected file/);
  await write(root,'WSOCK32.dll','tampered');await assert.rejects(r.installArchive(list,root,'p'),/checksum differs/);
});
test('reject original-game and out-of-root framework destinations',()=>{
  for(const name of ['MT2.exe','SDL3.dll','Data/MMORPG.zip','../mod.json']) assert.throws(()=>r.frameworkName(name));
});
test('Vortex profile and game paths isolate settings and worlds',()=>{
  const local=path.resolve('local-data'),game=path.resolve('game');
  assert.notEqual(r.profilePath(game,'one',local),r.profilePath(game,'two',local));
  assert.notEqual(r.profilePath(game,'one',local),r.profilePath(path.resolve('other'),'one',local));
  assert.throws(()=>r.profilePath(game,'../escape',local));
});
test('unknown and modified loaders remain untouched',async()=>{
  const game=await fixture();await write(game,'WSOCK32.dll','foreign');
  await assert.rejects(r.verifyOwnership(game),/unknown or modified/);
  assert.equal((await fs.readFile(path.join(game,'WSOCK32.dll'))).toString(),'foreign');
  await write(game,r.OWNER,{owner:'mt2fw-vortex',files:{'WSOCK32.dll':r.hash('original')}});
  await assert.rejects(r.verifyOwnership(game),/unknown or modified/);
});
test('retained manual profile configuration is backed up before transition',async()=>{
  const game=await fixture(),profile=await fixture();const original=JSON.stringify({profile:'previous-profile',enabled:true});
  await write(game,'mt2fw.json',original);await r.verifyOwnership(game,{},profile);
  assert.equal((await fs.readFile(path.join(profile,'backups/vortex-transition/configuration-'+r.hash(original)+'.json'))).toString(),original);
  assert.equal((await fs.readFile(path.join(game,'mt2fw.json'))).toString(),original);
});
test('exact executable identity is required',async()=>{
  const game=await fixture();await write(game,'MT2.exe','fixture');
  await r.assertGame(game,r.hash('fixture'));await assert.rejects(r.assertGame(game),/Unsupported/);
});
