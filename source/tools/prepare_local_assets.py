from pathlib import Path
import json,re,shutil,sys
source=Path(sys.argv[1]).resolve();editor=Path(__file__).resolve().parents[1]/'editor';generated=editor/'generated';generated.mkdir(exist_ok=True);data={};decoder=json.JSONDecoder()
names={'MESSAGES','VOICE_CATALOG','EnemyLibraryData','OriginalEnemyMovementData','SceneryLibraryData','CombatGroupCatalog','SAMPLE','NATIVE_CATALOG'}
for name in ['dialogue.js','model-preview.js','sample.js']:
 for line in (source/name).read_text(encoding='utf-8').splitlines():
  match=re.match(r'(?:window\.([A-Za-z0-9_]+)|const ([A-Z][A-Z0-9_]+))\s*=\s*([\[{])',line)
  key=(match[1] or match[2]) if match else None
  if key in names:data[key]=decoder.raw_decode(line[match.start(3):])[0]
if set(data)!=names:raise SystemExit('Use a prepared tester installation containing the generated game datasets.')
(generated/'game-data.js').write_text('window.StarForgeGameData='+json.dumps(data,separators=(',',':'))+';\n',encoding='utf-8')
for file in ['models.json','Corneria.starlevel',*['Corneria.starstage.json','Meteo.starstage.json','Sector-X.starstage.json','Area-6.starstage.json','Sector-Y.starstage.json','Venom-1.starstage.json','Solar.starstage.json','Zoness.starstage.json']]:
 if (source/file).exists():shutil.copy2(source/file,editor/file)
runtime=editor/'NativeGame';runtime.mkdir(exist_ok=True)
for file in ['Starship.exe','config.yml','starship.o2r','gamecontrollerdb.txt','sf64.o2r']:
 if (source/'NativeGame'/file).exists():shutil.copy2(source/'NativeGame'/file,runtime/file)
if (source/'NativeGame/assets').is_dir():shutil.copytree(source/'NativeGame/assets',runtime/'assets',dirs_exist_ok=True)
(runtime/'starship.cfg.json').write_text('{"CVars":{"gSpaceArmada":0,"gCorneriaRemix":0}}\n')
print('Imported ignored local game datasets and runtime. Personal projects, sessions, settings and saves were not copied.')
