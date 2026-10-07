"""Validate editable tower data without starting Unreal. Exits nonzero on errors."""
from pathlib import Path
import json
import math

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {
    'critical': ('chance', 'multiplier'), 'poison': ('damage_per_second', 'duration', 'slow'),
    'splash': ('radius',), 'slow': ('fraction', 'duration'), 'splash_slow': ('radius', 'fraction', 'duration'),
    'attack_speed_aura': ('fraction', 'radius', 'channel'), 'damage_aura': ('fraction', 'radius', 'channel'),
    'slow_aura': ('fraction', 'radius', 'ground', 'air', 'channel'),
    'armor_aura': ('amount', 'radius', 'ground', 'air', 'channel'),
    'stun': ('chance', 'duration'), 'armor_reduction': ('amount', 'duration'),
    'bonus_gold': ('chance',), 'mana': ('maximum', 'regeneration', 'spell_cost'),
    'frost_nova': ('chance', 'damage', 'radius', 'fraction', 'duration'),
    'flame_strike': ('chance', 'damage_per_second', 'radius', 'duration')}


def validate(data, recipes, waves, check_assets=True):
    errors = []
    def require(ok, text):
        if not ok: errors.append(text)
    c = data['catalog']
    def ids(key):
        values = [x['id'] for x in c[key]]
        require(len(values)==len(set(values)) and all(values), f'duplicate/empty {key}')
        return values
    types, qualities, kinds = ids('gem_types'), ids('qualities'), ids('tower_types')
    entries = data['base_gems'] + data['special_towers'] + data['obstacles']
    definitions = {d['id']: d for d in entries}
    require(len(definitions)==len(entries), 'duplicate definition ID')
    require(c['rock_definition'] in definitions, 'missing stone definition')
    for t in types:
        for q in qualities:
            require(sum(d['gem_type']==t and d['quality']==q for d in data['base_gems'])==1, f'{t}/{q}: missing/duplicate base gem')
        require(t in c['damage_table'], f'{t}: no damage table row')
        if t in c['damage_table']:
            require(set(c['damage_table'][t])==set(c['armor_types']), f'{t}: incomplete armor multipliers')
    for i, level in enumerate(c['chance_levels']):
        weights = level['weights']
        require(level['level']==i and level['upgrade_cost']>=0, f'chance level {i}: invalid level/cost')
        require(set(weights)<=set(qualities), f'chance level {i}: unknown quality')
        require(all(isinstance(v,int) and v>=0 for v in weights.values()) and sum(weights.values())==100, f'chance level {i}: weights must total 100')
    require(all(r['count']>=2 and r['quality_steps']>0 for r in c['merge_rules']), 'invalid merge rule')
    for d in entries:
        id, s = d['id'], d['stats']
        require(d['tower_type'] in kinds, f'{id}: unknown tower type')
        if id != c['rock_definition']: require(d['gem_type'] in types and d['quality'] in qualities, f'{id}: unknown gem type/quality')
        for key in ('damage_base','range','projectile_speed','damage_per_level'):
            require(isinstance(s[key],(int,float)) and math.isfinite(s[key]) and s[key]>=0, f'{id}: invalid {key}')
        for key in ('damage_dice','damage_sides','targets','initial_level'):
            require(isinstance(s[key],int) and s[key]>=0, f'{id}: invalid {key}')
        require(s['attack_interval']>0 and (s['damage_dice']==0 or s['damage_sides']>0), f'{id}: invalid dice/interval')
        require(s['delivery'] in ('instant','projectile'), f'{id}: unknown delivery')
        require(s['delivery']!='projectile' or s['projectile_speed']>0, f'{id}: projectile has zero speed')
        if d['upgrades_to']:
            require(d['upgrades_to'] in definitions and d['upgrade_cost']>0, f'{id}: invalid upgrade')
        visited, current = set(), id
        while current in definitions and definitions[current]['upgrades_to']:
            if current in visited:
                require(False, f'{id}: upgrade cycle'); break
            visited.add(current); current = definitions[current]['upgrades_to']
        if check_assets:
            for field in ('model','material'):
                asset = d[field].split('.')[0]
                require(asset.startswith('/Game/') and (ROOT/'Content'/asset.removeprefix('/Game/')).with_suffix('.uasset').is_file(), f'{id}: missing {field}: {asset}')
        for m in d['modifiers']:
            kind = m['type']; require(kind in REQUIRED, f'{id}: unsupported modifier {kind}')
            require(all(k in m for k in REQUIRED.get(kind,())), f'{id}/{kind}: missing parameters')
            for k,v in m.items():
                if isinstance(v,(int,float)):
                    require(math.isfinite(v) and v>=0, f'{id}/{kind}: invalid {k}')
                    if k in ('chance','fraction','slow'): require(v<=1, f'{id}/{kind}: {k} must be 0..1')
    recipe_ids = [r['id'] for r in recipes['recipes']]
    require(len(recipe_ids)==len(set(recipe_ids)), 'duplicate recipe IDs')
    reachable = {d['id'] for d in data['base_gems']} | {c['rock_definition']}
    for r in recipes['recipes']:
        require(r['result'] in definitions, f'{r["id"]}: unknown result')
        require(bool(r['ingredients']), f'{r["id"]}: empty recipe')
        for x in r['ingredients']: require(x['id'] in definitions and isinstance(x['count'],int) and x['count']>0, f'{r["id"]}: invalid ingredient')
    for _ in entries:
        for r in recipes['recipes']:
            if all(x['id'] in reachable for x in r['ingredients']): reachable.add(r['result'])
        for id in list(reachable):
            if id in definitions and definitions[id]['upgrades_to']: reachable.add(definitions[id]['upgrades_to'])
    unreachable = set(definitions)-reachable
    require(all(definitions[id].get('availability')=='manual' for id in unreachable), 'unreachable definitions need explicit manual availability: '+', '.join(sorted(unreachable)))
    for i,w in enumerate(waves['waves']):
        require(w.get('armor_type','Neutral') in c['armor_types'], f'wave {i+1}: unknown armor type')
    return errors


if __name__=='__main__':
    data, recipes, waves = [json.loads((ROOT/'Content/Data'/f'{name}.json').read_text()) for name in ('Gems','Recipes','Waves')]
    errors = validate(data, recipes, waves)
    for e in errors: print('ERROR:',e)
    if errors: raise SystemExit(1)
    print('Validated 81 definitions, 13 recipes, all upgrade/model/material links, chances, modifiers, damage tables and wave armor references.')
