import unreal, json
from pathlib import Path
root=Path(__file__).resolve().parent
board=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.load_class(None,'/Script/GemTowerDefense.GemBoard'),unreal.Vector())
board.load_default_layout()
p=lambda x,y:unreal.IntPoint(x,y)
checks=[]
def check(name,result):
    assert result,name
    checks.append(name)
check('First cell fits',board.can_place(p(0,0)))
check('Last cell fits',board.can_place(p(38,38)))
check('Left border rejects protrusion',not board.can_place(p(-1,0)))
check('Right border rejects protrusion',not board.can_place(p(39,0)))
check('Top border rejects protrusion',not board.can_place(p(0,39)))
board.place(p(10,10),0)
check('Same location is blocked',not board.can_place(p(10,10)))
check('Half-cell overlap is blocked',not board.can_place(p(11,10)))
check('Diagonal overlap is blocked',not board.can_place(p(11,11)))
check('Touching edges allowed',board.can_place(p(12,10)))
check('Half-cell offset outside footprint allowed',board.can_place(p(12,11)))
board.remove(p(10,10))
check('Removal frees footprint',board.can_place(p(10,10)))
pos=board.placement_position(p(3,7))
check('Half-cell world mapping',abs(pos.x+800)<.001 and abs(pos.y+600)<.001)
check('World position snaps correctly',board.snap(pos)==p(3,7))
board.load_default_layout()
tiles=board.get_editor_property('tiles')
check('Exactly 400 typed tiles',len(tiles)==400)
types={str(t.get_editor_property('type')) for t in tiles}
check('Map has snow/grass, roads, intersections and checkpoints',all(str(t) in types for t in [unreal.GroundType.ROAD,unreal.GroundType.INTERSECTION,unreal.GroundType.CHECKPOINT]))
counts={t:sum(tile.get_editor_property('type')==t for tile in tiles) for t in [unreal.GroundType.SNOW,unreal.GroundType.GRASS,unreal.GroundType.ROAD,unreal.GroundType.INTERSECTION,unreal.GroundType.CHECKPOINT,unreal.GroundType.ENTRY,unreal.GroundType.EXIT]}
check('Exact ASCII tile counts',[counts[unreal.GroundType.SNOW]+counts[unreal.GroundType.GRASS],counts[unreal.GroundType.ROAD],counts[unreal.GroundType.INTERSECTION],counts[unreal.GroundType.CHECKPOINT],counts[unreal.GroundType.ENTRY],counts[unreal.GroundType.EXIT]]==[332,41,19,6,1,1])
expected=(root/'Maps/BoardLayout.txt').read_text().splitlines()
legend={'S':unreal.GroundType.SNOW,'R':unreal.GroundType.ROAD,'I':unreal.GroundType.INTERSECTION,'C':unreal.GroundType.CHECKPOINT,'E':unreal.GroundType.ENTRY,'X':unreal.GroundType.EXIT}
legend.update({n:unreal.GroundType.CHECKPOINT for n in '123456789'})
check('Every tile matches its ASCII coordinate',all(tile.get_editor_property('type') in [unreal.GroundType.SNOW,unreal.GroundType.GRASS] if expected[i//20][i%20]=='S' else tile.get_editor_property('type')==legend[expected[i//20][i%20]] for i,tile in enumerate(tiles)))
check('Entry and exit match the supplied map',board.entry_cell==p(0,2) and board.exit_cell==p(19,16))
check('Entry and exit cannot be covered',all(not board.can_place(cell) for cell in [p(0,4),p(1,4),p(38,32),p(37,32)]))
layers={str(c.get_name()):c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent)}
check('Entry and exit use the road appearance',layers['Tiles_5'].get_material(0)==layers['Tiles_2'].get_material(0)==layers['Tiles_6'].get_material(0))
check('Default route connects E through checkpoints to X',board.has_valid_route())
check('Default checkpoint order follows numeric labels',list(board.checkpoint_order)==[p(3,2),p(3,10),p(15,10),p(15,2),p(9,2),p(9,16)])

directions=[(1,0),(-1,0),(0,1),(0,-1)]
layouts=set()
for trial in range(100):
    board.regenerate()
    rows=list(board.get_editor_property('layout_rows'))
    assert len(rows)==20 and all(len(r)==20 for r in rows)
    assert all(sum(r.count(n) for r in rows)==1 for n in '123456')
    assert sum(r.count('I') for r in rows)==18
    endpoints={symbol:[(x,y) for y in range(20) for x in range(20) if rows[y][x]==symbol] for symbol in 'EX'}
    for symbol,points in endpoints.items():
        assert len(points)==1
        x,y=points[0]
        assert x in (0,19) or y in (0,19)
        assert not board.can_place(p(x*2,y*2))
    assert board.entry_cell==p(*endpoints['E'][0]) and board.exit_cell==p(*endpoints['X'][0])
    assert board.has_valid_route()
    centers=[(x,y) for n in '123456' for y in range(20) for x in range(20) if rows[y][x]==n]
    assert list(board.checkpoint_order)==[p(x,y) for x,y in centers]
    for x,y in centers:
        assert sum(rows[y+dy][x+dx]=='I' for dx,dy in directions)==3
        assert sum(rows[y+dy][x+dx]=='S' for dx,dy in directions)==1
    walkable={(x,y) for y in range(20) for x in range(20) if rows[y][x] in 'RI123456EX'}
    seen={centers[0]}; queue=[centers[0]]
    for x,y in queue:
        for dx,dy in directions:
            n=(x+dx,y+dy)
            if n in walkable and n not in seen: seen.add(n); queue.append(n)
    assert seen==walkable, 'Disconnected road network'
    layouts.add('\n'.join(rows))
check('100 generated maps have exactly six intact numbered T checkpoints',True)
check('All checkpoint roads are connected in 100 maps',True)
check('All 100 maps have one protected entry and exit on the boundary, connected to the road network',True)
check('Regenerate produces varied maps',len(layouts)>95)
board.load_default_layout()
check('Restore default recovers supplied array',list(board.get_editor_property('layout_rows'))==expected)
camera=unreal.new_object(unreal.load_class(None,'/Script/GemTowerDefense.GemCameraHandler'))
camera.orbit(unreal.Vector2D(120,-500))
check('Camera rotates and clamps pitch above the board',abs(camera.yaw+15)<.001 and camera.pitch==-80)
camera.pan(unreal.Vector2D(50,25),2)
check('Camera pans horizontally without changing altitude',camera.pan_offset.x**2+camera.pan_offset.y**2>1 and abs(camera.pan_offset.z)<.001)
camera.reset_view()
check('Camera reset restores the initial view',abs(camera.yaw+45)<.001 and abs(camera.pitch+35.264)<.001 and abs(camera.pan_offset.x)+abs(camera.pan_offset.y)+abs(camera.pan_offset.z)<.001)
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(board)
(root/'VerificationResults.json').write_text(json.dumps({'passed':checks},indent=2))
unreal.log('GEM_TEMPLATE_VERIFIED: '+str(len(checks))+' checks passed')
