// SPDX-License-Identifier: MIT
'use strict';
const {spawnSync} = require('node:child_process');
function steamAccount(query=()=>spawnSync('reg.exe',['query','HKCU\\Software\\Valve\\Steam\\ActiveProcess','/v','ActiveUser'],{encoding:'utf8',windowsHide:true})) {
  const result=query();
  if (result.error) return {status:'error',cause:result.error.message};
  if (result.status!==0) return {status:'unavailable',cause:(result.stderr || 'Steam active account is unavailable').trim()};
  const match=/ActiveUser\s+REG_DWORD\s+(0x[0-9a-f]+)/i.exec(result.stdout);
  if (!match) return {status:'error',cause:'Steam registry response has no ActiveUser DWORD'};
  const id=BigInt(match[1]);
  if (!id) return {status:'unavailable',cause:'Steam has no signed-in active account'};
  if (id>0xffffffffn) return {status:'error',cause:'Steam ActiveUser is outside the 32-bit account range'};
  return {status:'available',id:String(76561197960265728n+id)};
}
module.exports={steamAccount};
