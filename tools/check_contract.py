"""Check actual Lua exports, bridge operations and complete examples with the controlled native host."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,json,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mt2fw import packages
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--probe',type=Path,default=ROOT/'build/bin/Release/mt2fw_docs_probe.exe');args=parser.parse_args()
    evidence=ROOT/'artifacts/contracts'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');evidence.mkdir(parents=True)
    contract=packages.read_json(ROOT/'api/contract.json')
    bridge=(ROOT/'src/core/lua_mod.cpp').read_text(encoding='utf-8')
    host_names=set(re.findall(r"call\('([^']+)'",bridge))
    described={op for f in contract['functions'] for op in f['host_operations']}
    if host_names!=described:raise RuntimeError('Lua bridge operations differ: '+str(host_names^described))
    runtime=(ROOT/'src/core/core.cpp').read_text(encoding='utf-8')
    event_block=re.search(r'const std::set<std::string> events = \{([^}]+)\}',runtime)
    if not event_block:raise RuntimeError('Runtime event declaration format changed; update checker')
    actual_events=set(re.findall(r'"([^"]+)"',event_block[1]))
    if actual_events!={e['name'] for e in contract['events']}:raise RuntimeError('Runtime event names differ from contract')
    if packages.CAPABILITIES!=set(contract['capabilities']):raise RuntimeError('Package capabilities differ from contract')
    probe=evidence/'probe';probe.mkdir();(probe/'main.lua').write_text("for group, values in pairs(mt2) do for name, value in pairs(values) do if type(value) == 'function' then mt2.log.info('@export '..group..'.'..name) end end end\n",encoding='utf-8')
    packages.atomic_json(probe/'mod.json',{'id':'docs.exports','name':'Export probe','version':'1.0.0','api_version':'1.0.0','type':'lua','entry':'main.lua','capabilities':[],'multiplayer':'singleplayer'})
    rows=[]
    for index,directory in enumerate([probe]+sorted((ROOT/'examples').iterdir())):
        if directory!=probe and (not directory.is_dir() or packages.read_json(directory/'mod.json')['type']!='lua'):continue
        output=evidence/(directory.name+'.json');log=evidence/(directory.name+'.log')
        result=subprocess.run([str(args.probe),str(directory),str(output)],cwd=evidence,capture_output=True,text=True,encoding='utf-8',timeout=15)
        log.write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode:raise RuntimeError('Real Lua fixture failed: '+str(log))
        value=packages.read_json(output)
        if directory==probe:
            expected={f['name'][4:] for f in contract['functions']}
            if set(value['exports'])!=expected:raise RuntimeError('Actual loaded Lua exports differ: '+str(set(value['exports'])^expected))
        rows.append({'example':directory.name,'active':all(m['active'] for m in value['mods']),'receipt':output.name})
    summary={'exports':len(contract['functions']),'events':len(actual_events),'host_operations':len(host_names),'examples':rows,'fixture_only':True,'native_game_effects':'Retained game acceptance is separate; these checks run real Lua with a synthetic host.'}
    packages.atomic_json(evidence/'summary.json',summary);packages.atomic_json(ROOT/'artifacts/contract-current.json',{'evidence':str(evidence),'summary':summary})
    print('info: actual exports/events/bridge and '+str(len(rows)-1)+' complete Lua examples checked; evidence='+str(evidence))
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError,subprocess.TimeoutExpired) as error:print('error: '+str(error),file=sys.stderr);sys.exit(1)
