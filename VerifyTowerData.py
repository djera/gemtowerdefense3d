"""Unreal commandlet integration test: an added gem type requires no C++ edit.

Run while the project editor is closed. The temporary data edit is always restored.
"""
import copy
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parent
path = root/'Content/Data/Gems.json'
original = path.read_bytes()
data = json.loads(original)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
board = actors.spawn_actor_from_class(unreal.load_class(None,'/Script/GemTowerDefense.GemBoard'),unreal.Vector())
checks = []

def check(name, success):
    assert success, name
    checks.append(name)

try:
    board.load_default_layout()
    check('All original definitions validate in Unreal',not list(board.definition_errors))
    data['catalog']['gem_types'].append({'id':'EditableProbe','name':'Editable probe gem'})
    data['catalog']['damage_table']['EditableProbe'] = copy.deepcopy(data['catalog']['damage_table']['Aquamarine'])
    for definition in list(data['base_gems']):
        if definition['gem_type'] != 'Aquamarine': continue
        d = copy.deepcopy(definition)
        d['id'] = 'EditableProbe_'+d['quality']
        d['gem_type'] = 'EditableProbe'
        d['name'] = 'Edited '+d['quality']
        d['stats']['damage_base'] = 1234
        d['modifiers'] = [{'type':'slow','fraction':.42,'duration':7}]
        d['custom_data'] = {'nested_variable':'VISIBLE_CUSTOM_VALUE'}
        data['base_gems'].append(d)
    path.write_text(json.dumps(data),encoding='utf-8')
    board.reload_definitions()
    check('Adding a ninth gem type validates without rebuilding',not list(board.definition_errors))
    board.place(unreal.IntPoint(8,14),8,0)
    check('Numeric compatibility API uses catalog order',str(board.pieces[0].definition_id)=='EditableProbe_Chipped')
    board.select_at(unreal.IntPoint(8,14))
    lines = '\n'.join(board.selected_info())
    check('Edited stats are used by selection summary','1235 - 1237' in lines)
    check('New nested variables appear automatically','VISIBLE_CUSTOM_VALUE' in lines)
    check('Edited modifier values appear automatically','0.42' in lines and 'duration: 7' in lines)
    board.clear_all_gems()
    board.place_definition(unreal.IntPoint(8,14),'EditableProbe_Great')
    check('String-ID placement resolves newly defined Great tower',str(board.pieces[0].definition_id)=='EditableProbe_Great')
finally:
    path.write_bytes(original)
    actors.destroy_actor(board)

(root/'TowerDataVerificationResults.json').write_text(json.dumps({'passed':checks,'count':len(checks)},indent=2))
unreal.log(f'GEM_DATA_EDIT_CHECKS: {len(checks)} passed; original data restored')
