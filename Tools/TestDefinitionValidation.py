"""Check that broken editable connections are detected instead of silently ignored."""
import copy
import json
import unittest
from ValidateDefinitions import ROOT, validate

class DefinitionValidationTests(unittest.TestCase):
    def setUp(self):
        self.g, self.r, self.w = [json.loads((ROOT/'Content/Data'/f'{n}.json').read_text()) for n in ('Gems','Recipes','Waves')]
    def errors(self): return validate(self.g,self.r,self.w)
    def test_current_import(self): self.assertEqual([],self.errors())
    def test_missing_recipe_input(self):
        self.r['recipes'][0]['ingredients'][0]['id']='MissingGem'
        self.assertTrue(any('ingredient' in e for e in self.errors()))
    def test_upgrade_cycle(self):
        d=self.g['special_towers'][0]; d['upgrades_to']=d['id']; d['upgrade_cost']=1
        self.assertTrue(any('cycle' in e for e in self.errors()))
    def test_invalid_chance_table(self):
        self.g['catalog']['chance_levels'][0]['weights']['Great']=1
        self.assertTrue(any('total 100' in e for e in self.errors()))
    def test_unknown_modifier(self):
        self.g['base_gems'][0]['modifiers'].append({'type':'no_handler'})
        self.assertTrue(any('unsupported modifier' in e for e in self.errors()))
    def test_missing_modifier_parameter(self):
        self.g['base_gems'][0]['modifiers'].append({'type':'slow','fraction':.2})
        self.assertTrue(any('missing parameters' in e for e in self.errors()))
    def test_missing_asset(self):
        self.g['base_gems'][0]['model']='/Game/Missing.Missing'
        self.assertTrue(any('missing model' in e for e in self.errors()))
    def test_reordered_catalog_and_new_type(self):
        self.g['catalog']['gem_types'].reverse()
        self.g['catalog']['gem_types'].append({'id':'NewType','name':'New type'})
        self.g['catalog']['damage_table']['NewType']=copy.deepcopy(self.g['catalog']['damage_table']['Ruby'])
        for d in list(self.g['base_gems']):
            if d['gem_type']=='Ruby':
                clone=copy.deepcopy(d); clone['id']='NewType_'+d['quality']; clone['gem_type']='NewType'
                self.g['base_gems'].append(clone)
        self.assertEqual([],self.errors())

if __name__=='__main__': unittest.main()
