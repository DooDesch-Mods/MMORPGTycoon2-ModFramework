'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const os=require('node:os');
const path=require('node:path');
const {createCopyDeployment,JOURNAL}=require('../integrations/vortex/extension/copy-deployment');
const routing=require('../integrations/vortex/extension/routing');
async function fixture() {
  const root=await fs.mkdtemp(path.join(os.tmpdir(),'mt2fw-copy-'));
  const staging=path.join(root,'staging'),game=path.join(root,'game');
  await fs.mkdir(path.join(staging,'one'),{recursive:true});await fs.mkdir(game);
  await fs.writeFile(path.join(staging,'one','mod.json'),'first');
  return {root,staging,game,method:createCopyDeployment()};
}
async function deploy(f,previous=[],clean=true) {
  await f.method.prepare(f.game,clean,previous,x=>x.toLowerCase());
  await f.method.activate(path.join(f.staging,'one'),'one','',new Set());
  return f.method.finalize('mmorpgtycoon2',f.game,f.staging);
}
test('copy backend deploys, updates, disables and purges while preserving user files and staging',async()=>{
  const f=await fixture();await fs.writeFile(path.join(f.game,'save.world'),'keep');
  const files=await deploy(f);assert.equal(files.length,1);
  assert.equal(await f.method.isDeployed(f.staging,f.game,files[0]),true);
  assert.deepEqual(await f.method.externalChanges('mmorpgtycoon2',f.staging,f.game,files),[]);
  await fs.writeFile(path.join(f.staging,'one','mod.json'),'second');
  assert.deepEqual(await f.method.externalChanges('mmorpgtycoon2',f.staging,f.game,files),[]);
  const updated=await deploy(f,files);
  assert.equal(await fs.readFile(path.join(f.game,'mod.json'),'utf8'),'second');
  await f.method.prepare(f.game,false,updated,x=>x);
  await f.method.deactivate(path.join(f.staging,'one'),'','one');
  assert.deepEqual(await f.method.finalize('mmorpgtycoon2',f.game,f.staging),[]);
  assert.equal(await fs.readFile(path.join(f.staging,'one','mod.json'),'utf8'),'second');
  await deploy(f);await f.method.purge(f.staging,f.game);
  assert.equal(await fs.readFile(path.join(f.game,'save.world'),'utf8'),'keep');
  await assert.rejects(fs.stat(path.join(f.game,'mod.json')),/ENOENT/);
});
test('unowned or modified targets stop writes and remain available for Vortex change handling',async()=>{
  const f=await fixture();await fs.writeFile(path.join(f.game,'mod.json'),'foreign');
  await assert.rejects(deploy(f),/unknown or modified/);
  assert.equal(await fs.readFile(path.join(f.game,'mod.json'),'utf8'),'foreign');
  await fs.unlink(path.join(f.game,'mod.json'));const files=await deploy(f);
  await fs.writeFile(path.join(f.game,'mod.json'),'edited');
  assert.equal((await f.method.externalChanges('mmorpgtycoon2',f.staging,f.game,files))[0].changeType,'refchange');
  await assert.rejects(f.method.purge(f.staging,f.game),/modified file/);
  assert.equal(await fs.readFile(path.join(f.game,'mod.json'),'utf8'),'edited');
});
test('purge with no deployment preserves an unfamiliar loader and records no ownership',async()=>{
  const f=await fixture();await fs.writeFile(path.join(f.game,'WSOCK32.dll'),'foreign');
  await f.method.purge(f.staging,f.game);
  assert.equal(await fs.readFile(path.join(f.game,'WSOCK32.dll'),'utf8'),'foreign');
  await assert.rejects(fs.stat(path.join(f.game,JOURNAL)),/ENOENT/);
});
test('existing hardlinks migrate only with matching filesystem identity',async()=>{
  const f=await fixture(),file={source:'one',relPath:'mod.json',target:'',time:0};
  await fs.link(path.join(f.staging,'one','mod.json'),path.join(f.game,'mod.json'));
  const files=await deploy(f,[file]);
  await fs.writeFile(path.join(f.staging,'one','mod.json'),'new');
  await deploy(f,files);
  assert.equal(await fs.readFile(path.join(f.game,'mod.json'),'utf8'),'new');
  const g=await fixture();await fs.copyFile(path.join(g.staging,'one','mod.json'),path.join(g.game,'mod.json'));
  await assert.rejects(deploy(g,[file]),/unknown or modified/);
});
test('interrupted committed copy is recovered before purge',async()=>{
  const f=await fixture();await fs.writeFile(path.join(f.game,'mod.json'),'first');
  const hash=routing.hash('first'),row={name:'mod.json',hash,source:'one'};
  await fs.writeFile(path.join(f.game,JOURNAL),JSON.stringify({format:1,owner:'mt2fw-copy',target:f.game,staging:f.staging,files:{},pending:{name:'mod.json',before:null,after:hash,row}}));
  await f.method.purge(f.staging,f.game);
  await assert.rejects(fs.stat(path.join(f.game,'mod.json')),/ENOENT/);
});
test('unsafe deployment paths and symlinks cannot escape the target',async()=>{
  const f=await fixture();
  await f.method.prepare(f.game,true,[],x=>x);
  await assert.rejects(f.method.activate(path.join(f.staging,'one'),'../escape','',new Set()),/Unsafe/);
  await f.method.cancel();
  await fs.symlink(f.staging,path.join(f.game,'linked'),'junction');
  await f.method.prepare(f.game,true,[],x=>x);
  await f.method.activate(path.join(f.staging,'one'),'one','linked',new Set());
  await assert.rejects(f.method.finalize('mmorpgtycoon2',f.game,f.staging),/Symbolic link/);
});
test('absent Vortex merge output is optional but missing installed packages are errors',async()=>{
  const f=await fixture();await f.method.prepare(f.game,true,[],x=>x);
  await f.method.activate(path.join(f.staging,'__merged.mt2fw-runtime'),'__merged.mt2fw-runtime','',new Set());
  await assert.rejects(f.method.activate(path.join(f.staging,'missing'),'missing','',new Set()),/ENOENT/);
  await f.method.cancel();
});
test('empty mod types sharing a target do not remove another type deployment',async()=>{
  const f=await fixture();await deploy(f);
  await f.method.prepare(f.game,true,[],x=>x);
  assert.deepEqual(await f.method.finalize('mmorpgtycoon2',f.game,f.staging),[]);
  assert.equal(await fs.readFile(path.join(f.game,'mod.json'),'utf8'),'first');
});
