'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const path=require('node:path');
const Module=require('node:module');
const routing=require('../integrations/vortex/extension/routing');

test('existing Vortex deployment validates mods with the native player checker',async()=>{
  const evidence=path.resolve(__dirname,'../artifacts/runtime-vortex');await fs.mkdir(evidence,{recursive:true});
  const root=await fs.mkdtemp(path.join(evidence,'case-')),game=path.join(root,'game'),staging=path.join(root,'staging');
  const runtime=path.join(staging,'runtime'),folder=path.join(staging,'mod','test.ui');
  await fs.mkdir(path.join(runtime,'mt2fw'),{recursive:true});await fs.mkdir(folder,{recursive:true});await fs.mkdir(game);
  const checker=path.resolve(__dirname,'../build/bin/Release/mt2fw.exe');
  await fs.copyFile(checker,path.join(runtime,'mt2fw','mt2fw.exe'));
  const files={};
  for(const name of ['WSOCK32.dll','MT2Framework.dll']) {
    await fs.writeFile(path.join(runtime,name),'fixture');files[name]=routing.hash('fixture');
  }
  files['mt2fw/mt2fw.exe']=routing.hash(await fs.readFile(checker));
  await fs.writeFile(path.join(runtime,routing.OWNER),JSON.stringify({owner:'mt2fw-vortex',files}));
  const manifest={id:'test.ui',name:'UI',version:'1.0.0',api_version:'1.0.0',type:'lua',entry:'main.lua',capabilities:['ui']};
  await fs.writeFile(path.join(folder,'mod.json'),JSON.stringify(manifest));
  await fs.writeFile(path.join(folder,'main.lua'),'error("validation must never execute this script")');
  const state={settings:{profiles:{activeProfileId:'test'},gameMode:{discovered:{mmorpgtycoon2:{path:game}}}},persistent:{
    profiles:{test:{gameId:'mmorpgtycoon2',modState:{runtime:{enabled:true},mod:{enabled:true}}}},
    mods:{mmorpgtycoon2:{runtime:{type:routing.FRAMEWORK_TYPE,installationPath:'runtime'},mod:{installationPath:'mod',attributes:{mt2fwModId:'test.ui'}}}}
  }};
  const originalLoad=Module._load,originalAssert=routing.assertGame;
  try {
    Module._load=function(name,...args){if(name==='vortex-api')return {util:{},actions:{},selectors:{installPathForGame:()=>staging}};return originalLoad.call(this,name,...args);};
    const extension=require('../integrations/vortex/extension/index');
    routing.assertGame=async candidate=>{assert.equal(candidate,game);};
    await extension.willDeploy({getState:()=>state},'test');
    const config=JSON.parse(await fs.readFile(path.join(runtime,'mt2fw.json'),'utf8'));
    assert.equal(config.profile,routing.profilePath(game,'test'));
    await fs.writeFile(path.join(folder,'mod.json'),JSON.stringify({...manifest,capabilities:['unknown']}));
    await assert.rejects(extension.willDeploy({getState:()=>state},'test'),/failed the shared CLI validation.*Unknown capability/s);
  } finally {Module._load=originalLoad;routing.assertGame=originalAssert;}
});

test('empty-profile recovery preserves an unrelated loader and installed packages keep ownership guards',async()=>{
  const root=path.resolve(__dirname,'../artifacts/runtime-vortex');await fs.mkdir(root,{recursive:true});
  const game=await fs.mkdtemp(path.join(root,'empty-profile-'));
  const loader=path.join(game,'WSOCK32.dll');await fs.writeFile(loader,'unrelated loader');
  const state={settings:{gameMode:{discovered:{mmorpgtycoon2:{path:game}}}},persistent:{
    profiles:{test:{gameId:routing.GAME_ID}},mods:{[routing.GAME_ID]:{}}}};
  const extension=require('../integrations/vortex/extension/index');
  assert.equal(await extension.willPurge({getState:()=>state},'test'),false);
  assert.equal(await fs.readFile(loader,'utf8'),'unrelated loader');
  state.persistent.mods[routing.GAME_ID].runtime={};
  await assert.rejects(extension.willPurge({getState:()=>state},'test'),/unknown or modified/);
  delete state.persistent.mods[routing.GAME_ID].runtime;
  await fs.writeFile(path.join(game,routing.OWNER),JSON.stringify({owner:'mt2fw-vortex',files:{}}));
  await assert.rejects(extension.willPurge({getState:()=>state},'test'),/unknown or modified/);
  await fs.writeFile(path.join(game,routing.OWNER),JSON.stringify({owner:'mt2fw-vortex',files:{'WSOCK32.dll':routing.hash('unrelated loader')}}));
  assert.equal(await extension.willPurge({getState:()=>state},'test'),undefined);
  assert.equal(await fs.readFile(loader,'utf8'),'unrelated loader');
});
