// SPDX-License-Identifier: MIT
'use strict';
const fs = require('node:fs/promises');
const syncFS = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {relative} = require('./routing');
const JOURNAL = '.mt2fw-copy-deployment.json';
const fail = message => { throw new Error('MT2 deployment: ' + message); };
const key = value => value.replace(/\\/g,'/').toLowerCase();
const output = file => relative(path.join(file.target || '',file.relPath));
async function stat(filename) {
  try { return await fs.lstat(filename); }
  catch (error) { if (error.code === 'ENOENT') return undefined; throw error; }
}
async function safe(root, name) {
  let current = path.resolve(root);
  for (const part of relative(name).split('/')) {
    current = path.join(current,part);
    const info = await stat(current);
    if (info?.isSymbolicLink()) fail('Symbolic link in deployment path: ' + current);
  }
  // Check the root and its ancestors as well as the relative path.
  let cursor = path.resolve(root);
  while (true) {
    if ((await stat(cursor))?.isSymbolicLink()) fail('Symbolic link in deployment root: ' + cursor);
    const parent = path.dirname(cursor); if (parent === cursor) break; cursor = parent;
  }
  return current;
}
async function digest(filename) {
  const info = await stat(filename);
  if (!info) return null;
  if (!info.isFile() || info.isSymbolicLink()) fail('Expected a regular file: ' + filename);
  const hash = crypto.createHash('sha256');
  for await (const chunk of syncFS.createReadStream(filename)) hash.update(chunk);
  return hash.digest('hex');
}
async function atomic(filename, bytes) {
  const temporary = filename + '.mt2fw-' + crypto.randomUUID();
  await fs.writeFile(temporary,bytes,{flag:'wx'});
  try { await fs.rename(temporary,filename); }
  catch (error) { error.message += '; retained temporary file: ' + temporary; throw error; }
}
async function save(root, record) {
  await atomic(await safe(root,JOURNAL),JSON.stringify(record,null,2));
}
async function journal(root, staging) {
  const filename = await safe(root,JOURNAL);
  const exists = await stat(filename);
  const record = exists ? JSON.parse(await fs.readFile(filename,'utf8')) : {
    format:1,owner:'mt2fw-copy',target:path.resolve(root),staging:path.resolve(staging),files:{},
  };
  if (record.format !== 1 || record.owner !== 'mt2fw-copy' || key(record.target) !== key(path.resolve(root)) || !record.files)
    fail('Invalid ownership record: ' + filename);
  for (const [name,row] of Object.entries(record.files)) {
    relative(name);
    if (!/^[a-f0-9]{64}$/.test(row.hash)) fail('Invalid ownership checksum: ' + name);
  }
  if (record.pending) {
    const {name,before,after,row} = record.pending;
    const actual = await digest(await safe(root,name));
    if (actual === after) {
      if (after === null) delete record.files[key(name)];
      else record.files[key(name)] = row;
    } else if (actual !== before) fail('Interrupted deployment encountered a changed file: ' + name);
    delete record.pending;
    await save(root,record);
  }
  return record;
}
async function adoptLinks(record, root, staging, files) {
  // A previous Vortex manifest alone is not proof of ownership. Require the
  // destination to be the exact hardlink to its recorded staging file.
  for (const file of files || []) {
    const name = output(file), normalized = key(name);
    if (record.files[normalized]) continue;
    const source = await safe(staging,path.join(relative(file.source),file.relPath));
    const destination = await safe(root,name);
    const a = await stat(source), b = await stat(destination);
    if (a?.isFile() && b?.isFile() && a.nlink > 1 && a.dev === b.dev && a.ino === b.ino) {
      record.files[normalized] = {name,hash:await digest(destination),source:file.source};
      await save(root,record);
    }
  }
}
async function change(record, root, name, row, source) {
  const normalized = key(name), destination = await safe(root,name);
  const actual = await digest(destination), owned = record.files[normalized];
  if (actual !== null && (!owned || actual !== owned.hash)) fail('Preserved unknown or modified file: ' + name);
  const after = row?.hash ?? null;
  const sourceInfo = source ? await stat(source) : undefined, destinationInfo = await stat(destination);
  const linked = sourceInfo && destinationInfo && sourceInfo.nlink > 1 && sourceInfo.dev === destinationInfo.dev && sourceInfo.ino === destinationInfo.ino;
  if (actual === after && !linked) {
    if (row) record.files[normalized] = row;
    else delete record.files[normalized];
    return;
  }
  await fs.mkdir(path.dirname(destination),{recursive:true});
  record.pending = {name,before:actual,after,row};
  await save(root,record);
  if (source) {
    const temporary = destination + '.mt2fw-' + crypto.randomUUID();
    await fs.copyFile(source,temporary,syncFS.constants.COPYFILE_EXCL);
    if (await digest(temporary) !== after) fail('Source changed while copying: ' + name + '; retained ' + temporary);
    await fs.rename(temporary,destination);
  } else if (actual !== null) await fs.unlink(destination);
  if (row) record.files[normalized] = row;
  else delete record.files[normalized];
  delete record.pending;
  await save(root,record);
}
function createCopyDeployment() {
  let context, queue = Promise.resolve();
  return {
    name:'MT2 protected deployment',description:'Deploys MT2 mods safely across drives. Keeps staging files and preserves unowned files.',
    priority:0,isFallbackPurgeSafe:false,
    isSupported(state,gameId) {
      if (gameId !== 'mmorpgtycoon2') return {description:()=> 'MT2 Framework only'};
      if (!state.settings.gameMode.discovered[gameId]?.path) return {description:()=> 'Discover MMORPG Tycoon 2 first.'};
      return undefined;
    },
    detailedDescription(t) { return t(this.description); },
    async userGate() {}, async prePurge() {}, async postPurge() {},
    async prepare(root,clean,previous,normalize) {
      let release;
      const pending = new Promise(resolve=>{release=resolve;});
      const ready = queue; queue = queue.then(()=>pending); await ready;
      context = {root,previous:previous || [],files:new Map(),normalize,release};
      if (!clean) for (const file of context.previous) context.files.set(key(output(file)),file);
    },
    async activate(sourcePath,sourceName,deployPath,blackList) {
      if (!context) fail('No deployment in progress');
      relative(sourceName);
      if (deployPath) relative(deployPath);
      // Vortex supplies an optional merge-output directory even when no merger
      // produced files. Missing installed mod directories still fail normally.
      if (sourceName.startsWith('__merged') && !(await stat(sourcePath))) return;
      async function walk(folder,prefix='') {
        for (const entry of await fs.readdir(folder,{withFileTypes:true})) {
          const name = prefix + entry.name, filename = await safe(sourcePath,name);
          if (entry.isDirectory()) await walk(filename,name+'/');
          else {
            if (!entry.isFile()) fail('Not a regular staged file: ' + name);
            if (blackList.has(context.normalize(path.join(sourceName,name)))) continue;
            const file = {relPath:name,source:sourceName,target:deployPath,time:(await fs.stat(filename)).mtimeMs};
            if (key(output(file)) === key(JOURNAL)) fail('Package contains reserved ownership file');
            context.files.set(key(output(file)),file);
          }
        }
      }
      await walk(sourcePath);
    },
    async deactivate(_sourcePath,target,sourceName) {
      if (!context) fail('No deployment in progress');
      for (const [name,file] of context.files) if (file.source === sourceName && (file.target || '') === (target || '')) context.files.delete(name);
    },
    async finalize(_gameId,root,staging,progress) {
      if (!context || key(root) !== key(context.root)) fail('Deployment target changed');
      const current = context;
      try {
        const record = await journal(root,staging);
        await adoptLinks(record,root,staging,current.previous);
        const wanted = [];
        for (const file of current.files.values()) {
          const name = output(file), source = await safe(staging,path.join(relative(file.source),file.relPath));
          const hash = await digest(source);
          if (!hash) fail('Missing staged file: ' + source);
          wanted.push({file,name,source,hash});
        }
        // Check every affected destination before the first mutation.
        for (const name of new Set([...Object.keys(record.files),...wanted.map(row=>key(row.name))])) {
          const owned = record.files[name], actual = await digest(await safe(root,owned?.name || name));
          if (actual !== null && (!owned || actual !== owned.hash)) fail('Preserved unknown or modified file: ' + name);
        }
        let count=0;
        // Several Vortex mod types can share the game root. This pass may
        // remove only files in its own previous deployment, not another type.
        for (const file of current.previous) {
          const name=output(file), row=record.files[key(name)];
          if (row && !current.files.has(key(name))) await change(record,root,row.name,null);
        }
        const result = [];
        for (const {file,name,source,hash} of wanted) {
          await change(record,root,name,{name,hash,source:file.source},source);
          result.push({...file,mt2fwCopyHash:hash}); progress?.(++count,wanted.length);
        }
        await save(root,record);
        return result;
      } finally {context=undefined;current.release();}
    },
    async cancel() { if (context) {context.release();context=undefined;} },
    async purge(staging,root) {
      if (!root) return;
      const record = await journal(root,staging);
      for (const name of await fs.readdir(root)) {
        if (!/^vortex\.deployment(?:\.[a-z0-9_-]+)?\.json$/i.test(name)) continue;
        const previous = JSON.parse(await fs.readFile(await safe(root,name),'utf8'));
        if (previous.gameId === 'mmorpgtycoon2' && key(previous.targetPath || '') === key(root)
          && ['mt2fw-hardlink','hardlink_activator'].includes(previous.deploymentMethod))
          await adoptLinks(record,root,staging,previous.files);
      }
      // Preserve all files if one managed destination has been edited.
      for (const row of Object.values(record.files)) {
        const actual = await digest(await safe(root,row.name));
        if (actual !== null && actual !== row.hash) fail('Preserved modified file during purge: ' + row.name);
      }
      for (const row of Object.values({...record.files})) await change(record,root,row.name,null);
      if (await stat(path.join(root,JOURNAL))) await save(root,record);
    },
    getDeployedPath(input) { return input; },
    async isDeployed(staging,root,file) {
      const record = await journal(root,staging), row = record.files[key(output(file))];
      return !!row && await digest(await safe(root,row.name)) === row.hash;
    },
    async externalChanges(_gameId,staging,root,files) {
      const changes=[];
      for (const file of files || []) {
        const source = await safe(staging,path.join(relative(file.source),file.relPath)), destination = await safe(root,output(file));
        const a = await stat(source), b = await stat(destination);
        let changeType;
        if (!a && b) changeType='srcdeleted';
        else if (a && !b) changeType='deleted';
        else if (a && b) {
          const expected=file.mt2fwCopyHash;
          if (expected ? await digest(destination) !== expected : !(a.nlink > 1 && a.dev === b.dev && a.ino === b.ino)) changeType='refchange';
        }
        if (changeType) changes.push({filePath:file.relPath,source:file.source,changeType,sourceTime:a?.mtime,destTime:b?.mtime});
      }
      return changes;
    },
  };
}
module.exports = {createCopyDeployment,JOURNAL};
