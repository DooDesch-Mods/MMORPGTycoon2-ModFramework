'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const r = require('../integrations/vortex/extension/routing');
const {createGuardedDeployment,checkActivator,ID} = require('../integrations/vortex/extension/guarded-deployment');
const {steamAccount} = require('../integrations/vortex/extension/steam-account');
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
test('Vortex can clear an unavailable method without selecting an unprotected one',()=>{
  for(const activatorId of [undefined,null,'',ID]) {
    assert.equal(checkActivator({}, {payload:{gameId:r.GAME_ID,activatorId}}),undefined);
  }
  for(const activatorId of ['hardlink_activator','symlink_activator_elevated']) {
    assert.match(checkActivator({}, {payload:{gameId:r.GAME_ID,activatorId}}),/protected hardlink/);
    assert.equal(checkActivator({}, {payload:{gameId:'other',activatorId}}),undefined);
  }
});
test('empty unmanaged profile skips both built-in purge methods without file operations',async()=>{
  const calls=[];
  const deployment=createGuardedDeployment(()=>({prePurge(){calls.push('prePurge');},purge(){calls.push('purge');}}),
    async()=>{},async()=>false,async()=>{});
  await deployment.prePurge();await deployment.purge('C:\\staging','F:\\game');
  assert.deepEqual(calls,[]);
});
test('cross-volume failure gives both paths and retains the built-in repair callback',()=>{
  const fixCallback=()=>{},baseProblem={description:()=> 'Original cause',order:5,fixCallback};
  const state={};
  const deployment=createGuardedDeployment(()=>({isSupported:()=>baseProblem}),null,null,null,undefined,
    ()=>({staging:'C:\\Vortex\\mods',game:'F:\\Steam\\MT2'}));
  const problem=deployment.isSupported(state,r.GAME_ID,'');
  assert.match(problem.description(),/C:.*F:/);assert.match(problem.solution(),/Settings > Mods/);
  assert.equal(problem.fixCallback,fixCallback);assert.equal(problem.order,5);
  const sameVolume=createGuardedDeployment(()=>({isSupported:()=>baseProblem}),null,null,null,undefined,
    ()=>({staging:'F:\\mods',game:'F:\\game'}));
  assert.equal(sameVolume.isSupported(state,r.GAME_ID,''),baseProblem);
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
test('classic model packs retain bytes and a stable identity across updates',async()=>{
  const root=await fixture(),name='More sceneries - lite mode/scenery/festival/bird.vmb';
  await write(root,name,Buffer.from('\0\x07ModelV2first-model'));
  const first=await r.installArchive([name],root,'profile');
  const id=first.instructions.find(row=>row.key==='mt2fwClassicId').value;
  assert.equal(first.instructions.at(-1).value,r.SCENERY_TYPE);
  assert.equal(first.instructions[0].destination,path.join(id,'scenery/festival/bird.vmb'));
  const record=JSON.parse(first.instructions.find(row=>row.type==='generatefile').data);
  assert.equal(record.files['scenery/festival/bird.vmb'],r.hash(await fs.readFile(path.join(root,name))));
  await write(root,name,Buffer.from('\0\x07ModelV2updated-model'));
  const second=await r.installArchive([name],root,'profile');
  assert.equal(second.instructions.find(row=>row.key==='mt2fwClassicId').value,id);
});
test('classic scenery rejects foreign payload, ambiguous roots and invalid models',async()=>{
  const root=await fixture(),one='Pack/scenery/prop/bird.vmb',two='Other/scenery/table/bird.vmb';
  await write(root,one,Buffer.from('\0\x07ModelV1fixture'));await write(root,two,Buffer.from('\0\x07ModelV2fixture'));
  await assert.rejects(r.installArchive([one,'Pack/buttons/build.xml'],root,'p'),/Unexpected file/);
  await assert.rejects(r.installArchive([one,two],root,'p'),/multiple classic/);
  await assert.rejects(r.installArchive(['scenery/prop/bird.vmb'],root,'p'),/named top-level/);
  await write(root,one,'not a model');await assert.rejects(r.installArchive([one],root,'p'),/VMB header/);
});
test('classic staging checks detect tampering and virtual-resource conflicts',async()=>{
  const root=await fixture(),id='classic.pack.12345678',name='scenery/prop/bird.vmb',bytes=Buffer.from('\0\x07ModelV2fixture');
  const record={format:1,id,name:'Pack',files:{[name]:r.hash(bytes)}};
  await write(root,id+'/'+r.SCENERY_RECORD,record);await write(root,id+'/'+name,bytes);
  assert.deepEqual(await r.checkScenery(root,id),record);
  r.checkSceneryConflicts([record,{...record,id:'classic.other.87654321',name:'Other',files:{'scenery/prop/other.vmb':r.hash(bytes)}}]);
  assert.throws(()=>r.checkSceneryConflicts([record,{...record,id:'classic.other.87654321',name:'Other'}]),/resource conflict.*Pack and Other/);
  assert.throws(()=>r.checkSceneryConflicts([record,record]),/Two enabled packages/);
  await write(root,id+'/'+name,'changed');await assert.rejects(r.checkScenery(root,id),/checksum differs/);
  await write(root,id+'/'+name,bytes);await write(root,id+'/buttons/override.win','extra');
  await assert.rejects(r.checkScenery(root,id),/Unexpected staged scenery file/);
});
test('classic paths follow the profile and exact Steam account',()=>{
  const profile=path.resolve('profile');
  assert.equal(r.sceneryPath(profile,'76561198000000001'),path.join(profile,'game-data','76561198000000001','mod'));
  assert.notEqual(r.sceneryPath(profile,'76561198000000001'),r.sceneryPath(profile,'76561198000000002'));
  assert.throws(()=>r.sceneryPath(profile,'../escape'));
  assert.ok(r.validSteamAccount('76561202255233023'));
  assert.equal(r.validSteamAccount('76561202255233024'),false);
});
test('Steam account lookup distinguishes signed-in, missing and failed results',()=>{
  assert.deepEqual(steamAccount(()=>({status:0,stdout:'ActiveUser REG_DWORD 0x1'})),{status:'available',id:'76561197960265729'});
  assert.equal(steamAccount(()=>({status:0,stdout:'ActiveUser REG_DWORD 0x0'})).status,'unavailable');
  assert.equal(steamAccount(()=>({status:1,stderr:'not found'})).status,'unavailable');
  assert.equal(steamAccount(()=>({error:new Error('query failed')})).cause,'query failed');
  assert.equal(steamAccount(()=>({status:0,stdout:'unexpected'})).status,'error');
});
test('Vortex detects the scenery type from file instructions before activation',async()=>{
  const Module=require('node:module'),original=Module._load,types=[];
  let entry;
  try {
    Module._load=function(name,...args){if(name==='vortex-api')return {util:{},selectors:{},actions:{}};return original.call(this,name,...args);};
    entry=require('../integrations/vortex/extension/index');
  }finally{Module._load=original;}
  entry.default({api:{},registerDeploymentMethod(){},registerActionCheck(){},registerGame(){},registerInstaller(){},registerModType(...args){types.push(args);}});
  const root=await fixture(),name='Pack/scenery/prop/bird.vmb';await write(root,name,Buffer.from('\0\x07ModelV2fixture'));
  const result=await r.installArchive([name],root,'p');
  const files=result.instructions.filter(row=>['copy','generatefile'].includes(row.type));
  assert.equal(await types.find(row=>row[0]===r.SCENERY_TYPE)[4](files),true);
  assert.equal(await types.find(row=>row[0]===r.FRAMEWORK_TYPE)[4](files),false);
});
