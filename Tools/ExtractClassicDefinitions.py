"""Extract factual configuration only; never executes the downloaded game code.
Input: ClassicRulesReference.js and ClassicReference.html (public Classic client).
Output: editable Content/Data/Gems.json and Recipes.json.
"""
from pathlib import Path
import re, json, hashlib
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/'ClassicRulesReference.js').read_text(encoding='utf-8')
html=(ROOT/'ClassicReference.html').read_text(encoding='utf-8')
types=['Amethyst','Aquamarine','Diamond','Emerald','Opal','Ruby','Sapphire','Topaz']
qualities=['Chipped','Flawed','Normal','Flawless','Perfect','Great']
def block(text,start):
    depth=0; quote=None; escaped=False
    for i in range(start,len(text)):
        c=text[i]
        if quote:
            if escaped: escaped=False
            elif c=='\\': escaped=True
            elif c==quote: quote=None
            continue
        if c in '\"\'`': quote=c; continue
        if c=='(': depth+=1
        if c==')':
            depth-=1
            if depth==0:return text[start+1:i],i+1
    raise ValueError('Unbalanced block')
def assignments(text):
    result={}
    for m in re.finditer(r'this\.(\w+)\s*=\s*(![01]|-?(?:\d*\.\d+|\d+)(?:e[+-]?\d+)?)(?=[,;)])',text):
        token=m[2]
        result[m[1]]=token=='!0' if token.startswith('!') else float(token)
    return result
reset_start=src.find('resetAbilities(){',1740000)
reset_end=src.find('}setStandardAttributes(){',reset_start)
defaults=assignments(src[reset_start:reset_end])
standard_start=reset_end+1
standard_end=src.find('applyGreat',standard_start)
standard=src[standard_start:standard_start+11000]
base_matches=list(re.finditer(r'(\d)==this\.towerBaseType(?:\?|&&)\(',standard))
enumstart=src.find('var js=')
enumend=src.find('function qs',enumstart)
enum={int(n):name for name,n in re.findall(r'e\[e\.(\w+)=(\d+)\]',src[enumstart:enumend])}
switchstart=src.find('setAttributes(){switch(this.towerType)')
switchend=src.find('resetAbilities(){',switchstart)
switch=src[switchstart:switchend]
cases={int(n):body for n,body in re.findall(r'case (\d+):(.*?);break;',switch,re.S)}
scale=100/26 # Original gem footprint = two 13px cells; Unreal footprint = 100cm.

def definition(id,name,p,quality=4):
    type_index=int(p.get('towerBaseType',0))
    mods=[]
    def mod(kind,**values): mods.append({'type':kind,**values})
    if p.get('critChance',0): mod('critical',chance=p['critChance'],multiplier=p['critMultiplier'])
    if p.get('poisonDuration',0): mod('poison',damage_per_second=p['poisonDamage'],duration=p['poisonDuration'],slow=p['poisonSlowModifier'])
    if p.get('aoe',False): mod('splash',radius=p['aoeRange']*scale,source_radius=p['aoeRange'])
    if p.get('iceSlow',False): mod('slow',fraction=p['iceSlowModifier'],duration=2)
    if p.get('aoeFreeze',False): mod('splash_slow',radius=p['aoeRange']*scale,fraction=p['iceSlowModifier'],duration=2)
    if p.get('speedAuraOpalValue',0): mod('attack_speed_aura',fraction=p['speedAuraOpalValue'],radius=p['speedAuraOpalRange']*scale,channel='haste')
    if p.get('speedAuraBoostValue',0): mod('attack_speed_aura',fraction=p['speedAuraBoostValue'],radius=p['speedAuraBoostRange']*scale,channel='boost')
    if p.get('speedAuraVitalityValue',0): mod('attack_speed_aura',fraction=p['speedAuraVitalityValue'],radius=p['speedAuraVitalityRange']*scale,channel='vitality')
    if p.get('damageAuraValue',0): mod('damage_aura',fraction=p['damageAuraValue'],radius=p['damageAuraRange']*scale,channel='yellow_sapphire' if 'Sapphire' in id else 'black_opal')
    if p.get('stunChance',0): mod('stun',chance=p['stunChance'],duration=p['stunDuration'])
    if p.get('armorPenalty',False): mod('armor_reduction',amount=p['armorPenaltyValue'],duration=p['armorPenaltyDuration'])
    if p.get('proxAuraArmorPenalty',0): mod('armor_aura',amount=p['proxAuraArmorPenalty'],radius=p['proxAuraRange']*scale,ground=p['proxAuraGround'],air=p['proxAuraFlying'])
    if p.get('proxAuraSpeedPenalty',0): mod('slow_aura',fraction=p['proxAuraSpeedPenalty'],radius=p['proxAuraRange']*scale)
    if p.get('damageBurn',False): mod('burn',damage_per_second={'StarRuby':40,'BloodStar':50,'FireStar':100}.get(id,0))
    if p.get('bonusGoldChance',0): mod('bonus_gold',chance=p['bonusGoldChance'],amount=1)
    if p.get('maxMana',0): mod('mana',maximum=p['maxMana'],regeneration=p['manaRegen'],spell_cost=p['spellCost'],spell_chance=p['spellChance'])
    if p.get('frostnovaDamage',0): mod('frost_nova',damage=p['frostnovaDamage'],radius=p['frostnovaAoeRange']*scale,chance=p['spellChance'])
    if p.get('flamestrikeDamage',0): mod('flame_strike',damage_per_second=p['flamestrikeDamage'],radius=p['flamestrikeAoeRange']*scale,duration=p['flamestrikeDuration'],chance=p['spellChance'])
    base=p.get('damageBase',1); dice=int(p.get('numDie',1)); sides=int(p.get('sidesPerDie',1))
    return {'id':id,'name':name,'gem_type':types[type_index],'quality':quality,'model':f'/Game/Gems/Meshes/SM_{types[type_index]}_{qualities[min(quality,4)]}.SM_{types[type_index]}_{qualities[min(quality,4)]}',
        'stats':{'damage_base':base,'damage_dice':dice,'damage_sides':sides,'damage_min':base+dice,'damage_max':base+dice*sides,'range':286*p['rangeModifier']*scale,'source_range':286*p['rangeModifier'],'attack_interval':p.get('cooldownModifier',1),'projectile_speed':500*p.get('projectileModifier',1)*scale,'targets':int(p.get('multiTargets',1)),'attacks_ground':p.get('attacksGround',True),'attacks_air':p.get('attacksFlying',True)},
        'modifiers':mods,'upgrade_cost':int(p.get('upgradeCost',0)),'upgrades_to':enum.get(int(p.get('upgradesTo',0)),'') if p.get('upgradesTo',0) else '', 'classic_properties':p}

base=[]
for m in base_matches[:8]:
    ti=int(m[1]); body,_=block(standard,m.end()-1)
    quality_matches=list(re.finditer(r'(\d)==this\.towerQuality(?:\?|&&)\(',body))
    assert len(quality_matches)==6,(ti,len(quality_matches))
    last_end=0; parsed=[]
    for q in quality_matches:
        b,last_end=block(body,q.end()-1); parsed.append((int(q[1]),assignments(b)))
    common=assignments(body[last_end:])
    for qi,values in parsed:
        p={**defaults,**values,**common,'towerBaseType':ti}
        base.append(definition(types[ti]+'_'+qualities[qi],qualities[qi]+' '+types[ti],p,qi))
assert len(base)==48,len(base)
special=[]
for number in range(2,34):
    if number not in cases: continue
    body=cases[number]; p={**defaults,**assignments(body)}
    if number in (13,14,15): p['cooldownModifier']=.45 if number==15 else .4
    name=enum[number]
    special.append(definition(name,re.sub(r'(?<!^)(?=[A-Z])',' ',name),p))
recipes=[]
section=html[html.index('<center id="special-gem-recipes">'):html.index('id="season-2-slate-recipes"')]
for row in re.findall(r'<tr>(.*?)</tr>',section,re.S):
    images=re.findall(r'/assets/(\w+)\.png',row)
    if len(images)<4: continue
    output=images[0]
    aliases={'uranium238':'Uranium235','bloodstone':'BloodStone'}
    id=aliases.get(output,next((x['id'] for x in special if x['id'].lower()==output),None))
    assert id,output
    ingredients=[]
    for item in images[1:]:
        q=next(q for q in qualities if item.startswith(q.lower()))
        t=next(t for t in types if item==q.lower()+t.lower())
        ingredients.append({'id':t+'_'+q,'count':1})
    recipes.append({'id':id,'name':next(x['name'] for x in special if x['id']==id),'result':id,'ingredients':ingredients})
assert len(recipes)==13,len(recipes)
out=ROOT/'Content/Data'; out.mkdir(parents=True,exist_ok=True)
metadata={'source':'https://www.gemtowerdefense.com/classic/game-1.0.5-legendary-preview-1.js','recipe_source':'https://www.gemtowerdefense.com/','source_sha256':hashlib.sha256(src.encode()).hexdigest(),'distance_scale':scale,'original_gem_footprint_pixels':26,'unreal_gem_footprint_cm':100,'notes':['Classic standard values and special tower ranks 2-33; Season 2 slates/fusions excluded.','Great tier retained for reference, with Perfect mesh fallback; ordinary rolls use five agreed tiers.','Slow duration 2 seconds is the original ice-slow behavior. Spell values and modifier lists retain source parameters.','Stats are base values before auras, enemy armor, difficulty, and critical rolls.']}
(out/'Gems.json').write_text(json.dumps({'metadata':metadata,'base_gems':base,'special_towers':special},indent=2),encoding='utf-8')
(out/'Recipes.json').write_text(json.dumps({'source':'https://www.gemtowerdefense.com/','recipes':recipes},indent=2),encoding='utf-8')
print(f'Extracted {len(base)} base gem definitions, {len(special)} special ranks, {len(recipes)} classic recipes.')
