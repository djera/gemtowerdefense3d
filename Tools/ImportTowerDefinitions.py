"""Import factual tower configuration from the pinned Objective-C reference.

No downloaded code is executed. Run deliberately: this overwrites Gems.json and
Recipes.json. Gameplay edits belong in those JSON files, not in this importer.
"""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[1]
REF = ROOT / 'Reference/GemTD'
OUT = ROOT / 'Content/Data'
source = (REF / 'Tower.m').read_text(encoding='utf-8')
header = (REF / 'Tower.h').read_text(encoding='utf-8')
revision = (REF / 'revision.txt').read_text().strip()
url = f'https://github.com/peterholko/gemtd/blob/{revision}/TowerDefenseUpdate/Tower.m'
constants = {k: float(v) for k, v in re.findall(r'#define (BASE_\w+) ([\d.]+)', header)}
scale = 100 / 26


def enum(name):
    body = re.search(r'typedef enum\s*\{([^}]+)\}\s*' + name, header)[1]
    return re.findall(r'\b\w+\b', body)


types, qualities, tower_types = map(enum, ['TowerBaseType', 'TowerQuality', 'TowerType'])


def block(text, start):
    """Balanced braces, excluding quoted strings. Input has comments removed."""
    depth, quote, escape = 0, None, False
    for i in range(start, len(text)):
        c = text[i]
        if quote:
            if escape: escape = False
            elif c == '\\': escape = True
            elif c == quote: quote = None
        elif c == '"': quote = c
        elif c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return text[start + 1:i], i + 1
    raise ValueError('Unbalanced source block')


# Strip line comments, retaining quoted descriptions (including apostrophes).
clean = re.sub(r'//[^\n]*', '', source)


def method(name):
    m = re.search(r'-\([^)]*\)' + name + r'\s*\{', clean)
    return block(clean, m.end() - 1)[0]


def assignments(text):
    result = {}
    for m in re.finditer(r'^\s*(\w+)\s*=\s*([^;\n]+)', text, re.M):
        key, expr = m[1], m[2].strip()
        if expr.lower() in ('true', 'false'): value = expr.lower() == 'true'
        elif expr in types + qualities + tower_types: value = expr
        elif re.fullmatch(r'-?[\d.]+(?:\s*/\s*BASE_\w+)?', expr):
            terms = expr.split('/')
            value = float(terms[0]) / (constants[terms[1].strip()] if len(terms) > 1 else 1)
            if value.is_integer(): value = int(value)
        elif expr.startswith('@"'): value = expr[2:-1]
        elif expr.startswith('[NSString stringWithFormat:'):
            value = re.search(r'@"([^"]*)"', text[m.start():])[1]
        else: continue
        result[key] = value
    return result


scalar_types = dict((name, kind) for kind, name in re.findall(r'\b(int|float|BOOL)\s+(\w+)\s*;', header))
defaults = {name: False if kind == 'BOOL' else 0 for name, kind in scalar_types.items()}
defaults.update(assignments(method('resetAbilities')))
defaults.update(towerLevel=0, numKills=0, attacking=True, upgradesTo='Rock', upgradeCost=0,
                towerBaseType=types[0], towerQuality=qualities[0], towerType='Standard',
                description='', selected=False, towerPlacedThisRound=False)
raw = []
standard = method('setStandardAttributes')
for match in re.finditer(r'if\(towerBaseType == (\w+)\)\s*\{', standard):
    name = match[1]
    body, _ = block(standard, match.end() - 1)
    spans, entries = [], []
    for q in re.finditer(r'(?:else )?if\(towerQuality == (\w+)\)\s*\{', body):
        b, end = block(body, q.end() - 1)
        spans.append((q.start(), end))
        entries.append((q[1], assignments(b)))
    common = body
    for start, end in reversed(spans): common = common[:start] + common[end:]
    for quality, fields in entries:
        p = {**defaults, **assignments(common), **fields, 'towerBaseType': name, 'towerQuality': quality}
        raw.append((f'{name}_{quality}', p))
assert len(raw) == 48
special = method('setAttributes')
for match in re.finditer(r'case (\w+):(.*?)break;', special, re.S):
    if match[1] in ('Rock', 'Standard'): continue
    raw.append((match[1], {**defaults, **assignments(match[2]), 'towerType': match[1]}))
assert len(raw) == 80

repairs = []


def repair(id, p, **changes):
    for field, value in changes.items():
        if p.get(field) != value:
            repairs.append({'id': id, 'field': field, 'source': p.get(field), 'effective': value})
            p[field] = value


definitions = []
for id, original in raw:
    p = dict(original)
    # Repair clear wiring mistakes; retain every original field for comparison.
    if id in ('Jade', 'AsianJade', 'LuckyAsianJade'): repair(id, p, poisonSlow=True)
    if id in ('Uranium238', 'Uranium235'):
        repair(id, p, proxAuraArmorPenalty=0, proxAuraSpeedPenalty=.5, damageBurn=True)
    if id == 'PinkDiamond': repair(id, p, upgradesTo='GreatPinkDiamond')
    if id == 'YellowSapphire': repair(id, p, upgradesTo='StarYellowSapphire', upgradeCost=210)
    if id == 'DarkEmerald': repair(id, p, stunDuration=1.5)
    if id == 'Sapphire_Chipped':
        repair(id, p, bonusAura=False, damageAuraSpecialValue=0, damageAuraSpecialRange=0)
    if id in ('RedCrystal', 'RedCrystalFacet', 'RoseQuartzCrystal'):
        repair(id, p, proxAuraArmorPenalty={'RedCrystal': 4, 'RedCrystalFacet': 5, 'RoseQuartzCrystal': 6}[id])
    if id in ('Paraiba', 'ParaibaTourmalineFacet'):
        repair(id, p, proxAuraGround=True, proxAuraRange=86 if id == 'Paraiba' else 93,
               proxAuraArmorPenalty=4 if id == 'Paraiba' else 6)
    t, q = p['towerBaseType'], p['towerQuality'] if p['towerType'] == 'Standard' else 'Perfect'
    mods = []
    def mod(kind, **values): mods.append({'type': kind, **values})
    if p['critChance']: mod('critical', chance=p['critChance'], multiplier=p['critMultiplier'])
    if p['poisonSlow']: mod('poison', damage_per_second=p['poisonDamage'], duration=p['poisonDuration'], slow=p['poisonSlowModidier'])
    if p['aoe']: mod('splash', radius=p['aoeRange'] * scale)
    if p['iceSlow']: mod('slow', fraction=p['iceSlowModifier'], duration=5)
    if p['aoeFreeze']: mod('splash_slow', radius=p['aoeRange'] * scale, fraction=p['iceSlowModifier'], duration=5)
    if p['speedAuraOpalValue']: mod('attack_speed_aura', fraction=p['speedAuraOpalValue'], radius=p['speedAuraOpalRange'] * scale, channel='opal')
    if p['damageAuraSpecialValue']: mod('damage_aura', fraction=p['damageAuraSpecialValue'], radius=p['damageAuraSpecialRange'] * scale, channel='special')
    if p['stunPossible']: mod('stun', chance=p['stunChance'], duration=p['stunDuration'])
    if p['armorPenalty']: mod('armor_reduction', amount=p['armorPenaltyValue'], duration=p['armorPenaltyDuration'])
    if p['proxAuraArmorPenalty']: mod('armor_aura', amount=p['proxAuraArmorPenalty'], radius=p['proxAuraRange'] * scale, ground=p['proxAuraGround'], air=p['proxAuraFlying'], channel='proximity')
    if p['proxAuraSpeedPenalty']: mod('slow_aura', fraction=p['proxAuraSpeedPenalty'], radius=p['proxAuraRange'] * scale, ground=p['proxAuraGround'], air=p['proxAuraFlying'], channel='proximity')
    if id == 'LuckyAsianJade': mod('bonus_gold', chance=.01, amount=0, wave_fraction=.5)
    if id in ('Paraiba', 'ParaibaTourmalineFacet'):
        mod('mana', maximum=10, regeneration=1.75 if id == 'Paraiba' else 2, spell_cost=5)
        mod('frost_nova', chance=.2, damage=200 if id == 'Paraiba' else 250, radius=29 * scale, fraction=.5, duration=5)
    asset_q = 'Perfect' if q == 'Great' else q
    stats = dict(damage_base=p['damageBase'], damage_dice=p['numDie'], damage_sides=p['sidesPerDie'],
                 range=286 * p['rangeModifier'] * scale, attack_interval=p['cooldownModifier'],
                 projectile_speed=500 * p['projectileModifier'] * scale, targets=p['multiTargets'],
                 attacks_ground=p['attacksGround'], attacks_air=p['attacksFlying'],
                 delivery='instant' if p['damageBurn'] else 'projectile', initial_level=0, damage_per_level=.1)
    name = f'{q} {t}' if p['towerType'] == 'Standard' else re.sub(r'(?<=[a-z])(?=[A-Z0-9])', ' ', id)
    entry = dict(id=id, name=name, gem_type=t, quality=q, tower_type=p['towerType'],
                 model=f'/Game/Gems/Meshes/SM_{t}_{asset_q}.SM_{t}_{asset_q}',
                 material=f'/Game/Prototype/Materials/M_Cel_{t}.M_Cel_{t}', stats=stats, modifiers=mods,
                 upgrades_to='' if p['upgradesTo'] == 'Rock' else p['upgradesTo'], upgrade_cost=p['upgradeCost'],
                 source_properties=original, import_repairs=[r for r in repairs if r['id'] == id])
    if q == 'Great': entry['model_note'] = 'Uses the Perfect mesh until a distinct Great model is assigned.'
    if id == 'UberStone': entry['availability'] = 'manual'
    definitions.append(entry)

catalog = dict(gem_types=[dict(id=t, name=t) for t in types],
               qualities=[dict(id=q, name=q, merge_enabled=True) for q in qualities],
               tower_types=[dict(id=t, name=t) for t in tower_types],
               rock_definition='Rock', merge_rules=[dict(count=2, quality_steps=1), dict(count=4, quality_steps=2)],
               chance_levels=[])
chances = [[100,0,0,0,0], [70,30,0,0,0], [60,30,10,0,0], [50,30,20,0,0],
           [40,30,20,10,0], [30,30,30,10,0], [20,30,30,20,0], [10,30,30,30,0], [0,30,30,30,10]]
for i, row in enumerate(chances):
    catalog['chance_levels'].append(dict(level=i, upgrade_cost=20 + 30 * i if i < 8 else 0,
                                         weights=dict(zip(qualities, row + [0]))))
damage_src = (REF / 'DamageTable.m').read_text()
table = {}
for t, armor, value in re.findall(r'table\[(\w+)\]\[(\w+)\] = ([\d.]+);', damage_src):
    table.setdefault(t, {'Neutral': 1})[armor] = float(value)
catalog['damage_table'] = table
catalog['armor_types'] = ['Neutral'] + list(next(iter(table.values())))[1:]
catalog['armor_positive'] = [float(v) for _, v in re.findall(r'armorTablePos\[(\d+)\] = ([\d.]+);', damage_src)]
catalog['armor_negative'] = [1] + [float(v) for _, v in re.findall(r'armorTableNeg\[(\d+)\] = ([\d.]+);', damage_src)]
rock = dict(id='Rock', name='Stone block', tower_type='Rock', gem_type='', quality='',
            model='/Game/Prototype/Meshes/SM_MazeStone.SM_MazeStone',
            material='/Game/Prototype/Materials/M_Cel_Stone.M_Cel_Stone',
            stats=dict(damage_base=0, damage_dice=0, damage_sides=0, range=0, attack_interval=1,
                       projectile_speed=0, targets=0, attacks_ground=False, attacks_air=False,
                       delivery='instant', initial_level=0, damage_per_level=0),
            modifiers=[], upgrades_to='', upgrade_cost=0)

# Recipe ingredients are imported from this repository's Game.m, repairing two typos.
game = (REF / 'Game.m').read_text()
recipe_body = game[game.index('-(void)initSpecialTowers'):game.index('-(void)authenticateLocalPlayer')]
recipes = []
for m in re.finditer(r'SpecialTower \*(\w+) = \[self specialTower:(\w+)\];(.*?)\[specialTowers addObject:\1\];', recipe_body, re.S):
    result = 'DarkEmerald' if m[1] == 'darkEmerald' else m[2]
    ingredients = []
    for q, t in re.findall(r'\[self standardTower:(\w+) _type:(\w+)\]', m[3]):
        if q not in qualities and t in qualities: q, t = t, q
        ingredients.append(dict(id=f'{t}_{q}', count=1))
    recipes.append(dict(id=result, name=next(d['name'] for d in definitions if d['id']==result), result=result, ingredients=ingredients))
assert len(recipes) == 13
metadata = dict(source=url, revision=revision, source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                constants=constants, distance_scale=scale, original_gem_footprint_pixels=26,
                unreal_gem_footprint_cm=100, notes=[
                    'stats, modifiers and catalog are authoritative editable gameplay values; source_properties are import provenance only.',
                    'All six qualities imported. Great is merge-only and reuses Perfect models.',
                    'Damage dice accumulate correctly. Source overwrote its roll and used integer level division.',
                    'Source state is reset on definition changes; stale abilities are not inherited.',
                    'Yellow Sapphire upgrade price 210 and missing Paraiba radius/mana parameters come from the previous classic definitions; spell damage/chance/cost follow the Tower.m comments. See ImportNotes.txt.',
                    'Waves remain the existing prototype waves, with Neutral armor type unless specified.'
                ])
OUT.mkdir(exist_ok=True, parents=True)
(OUT/'Gems.json').write_text(json.dumps(dict(schema_version=2, metadata=metadata, catalog=catalog,
    base_gems=definitions[:48], special_towers=definitions[48:], obstacles=[rock]), indent=2)+'\n', encoding='utf-8')
(OUT/'Recipes.json').write_text(json.dumps(dict(source=url.replace('Tower.m','Game.m'), notes=[
    'Dark Emerald recipe wrongly created BloodStone; corrected.',
    'Paraiba had the Flawed and Emerald arguments reversed; corrected. Uses the three ingredients in this source.'
], recipes=recipes), indent=2)+'\n', encoding='utf-8')
print(f'Imported {len(definitions)} towers, stone, {len(recipes)} recipes, {len(repairs)} documented field repairs.')
