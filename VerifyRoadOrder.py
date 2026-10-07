"""Check route coverage and shortest legs independently using Python BFS in Unreal."""
from collections import deque
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parent
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
board=actors.spawn_actor_from_class(unreal.load_class(None,'/Script/GemTowerDefense.GemBoard'),unreal.Vector())
checks=[]
p=lambda x,y:unreal.IntPoint(x,y)
xy=lambda point:(point.x,point.y)
directions=((1,0),(0,1),(-1,0),(0,-1))

def check(name,value):
    assert value,name
    checks.append(name)

def shortest(start,end,blocks):
    queue=deque([(start,0)]); seen={start}
    while queue:
        (x,y),distance=queue.popleft()
        if (x,y)==end: return distance
        for dx,dy in directions:
            q=(x+dx,y+dy)
            if 0<=q[0]<39 and 0<=q[1]<39 and q not in blocks and q not in seen:
                seen.add(q); queue.append((q,distance+1))
    return None

def assert_route(placements=()):
    order=list(board.road_order)
    rows=list(board.layout_rows)
    expected={(x,y) for y,row in enumerate(rows) for x,symbol in enumerate(row) if symbol in 'REXC123456789'}
    assert {xy(t.cell) for t in order}==expected
    assert [t.index for t in order]==list(range(1,len(order)+1))
    assert [t.checkpoint for t in order if t.checkpoint]>[]
    assert [t.checkpoint for t in order if t.checkpoint]==list(range(1,len(board.checkpoint_order)+1))
    assert xy(order[0].cell)==xy(board.entry_cell) and xy(order[-1].cell)==xy(board.exit_cell)
    goals=[(t.cell.x*2,t.cell.y*2) for t in order if not t.excluded]
    route=[xy(c) for c in board.debug_route_cells()]
    blocked={(x,y) for x in range(39) for y in range(39) if any(abs(x-bx)<2 and abs(y-by)<2 for bx,by in placements)}
    assert all(q not in blocked for q in route)
    cursor=0
    for goal in goals:
        cursor=route.index(goal,cursor)
    assert route[0]==goals[0] and route[-1]==goals[-1]
    expected_length=sum(shortest(a,b,blocked) for a,b in zip(goals,goals[1:]))
    assert len(route)-1==expected_length,(len(route)-1,expected_length)
    assert board.required_road_count()==len(goals) and board.has_valid_route()

try:
    board.load_default_layout()
    assert_route()
    required_count=sum(s in 'REXC123456789' for row in (root/'Content/Data/BoardLayout.txt').read_text().splitlines() for s in row)
    check('Default enumerates all editable R/checkpoint/E/X tiles, preserving checkpoint order',len(board.road_order)==required_count)
    check('Intersections are absent from required targets and debug numbering',all(board.layout_rows[t.cell.y][t.cell.x]!='I' for t in board.road_order))
    route=[xy(c) for c in board.debug_route_cells()]
    check('Shortest routes can traverse intersections', (4,4) in route)
    check('An intersection-only side arm does not force a detour', (8,4) not in route)
    check('Each route leg is shortest and every required road tile is visited in order',True)
    original=[(t.index,xy(t.cell)) for t in board.road_order]
    check('A quadrant-overlap road placement is permitted with a detour',board.can_place(p(19,13)))
    board.place(p(19,13),1,0)
    check('One half-cell quadrant excludes its whole road tile',board.road_is_obstructed(p(9,6)))
    check('An untouched adjacent road remains mandatory',not board.road_is_obstructed(p(9,5)))
    check('Road indices stay stable after placement',original==[(t.index,xy(t.cell)) for t in board.road_order])
    assert_route([(19,13)])
    check('Blocked road is skipped and remaining tiles use shortest detours',True)
    for x in (4,8,12,16): board.place(p(x,18),0,0)
    board.select_at(p(4,18)); board.keep_selected()
    check('A stone retains the same quadrant obstruction',board.pieces[0].rock and board.road_is_obstructed(p(9,6)))
    board.reset_run()
    check('Clearing gems restores road requirements and stable indices',board.required_road_count()==required_count and original==[(t.index,xy(t.cell)) for t in board.road_order])
    intersection_ring=((16,18),(20,18),(18,16),(18,20))
    for point in intersection_ring[:-1]:
        assert board.can_place(p(*point)),point
        board.place(p(*point),0,0)
    if board.layout_rows[9][9]=='I':
        assert board.can_place(p(*intersection_ring[-1]))
        board.place(p(*intersection_ring[-1]),0,0)
        check('An unreachable unobstructed intersection does not invalidate placement',not board.road_is_obstructed(p(9,9)) and board.has_valid_route())
        assert_route(intersection_ring)
    else:
        # The editable layout can promote this former intersection to a road.
        check('A road replacing the former intersection cannot be enclosed',not board.can_place(p(*intersection_ring[-1])))
        assert_route(intersection_ring[:-1])
    board.reset_run()
    for point in ((16,12),(20,12),(18,10)):
        assert board.can_place(p(*point)),point
        board.place(p(*point),0,0)
    check('Enclosing an unobstructed required road is rejected',not board.can_place(p(18,14)))
    check('Enclosed candidate does not count as overlapping the road',not board.road_is_obstructed(p(9,6)))
    board.reset_run()
    for point in ((4,4),(8,4),(6,2)):
        assert board.can_place(p(*point)),point
        board.place(p(*point),0,0)
    check('Enclosing a checkpoint is rejected',not board.can_place(p(6,6)))
    check('Covering a checkpoint is rejected',not board.can_place(p(6,4)))
    board.reset_run(); board.place(p(20,12),0,0)
    check('Edge-only contact does not exclude a neighboring road tile',not board.road_is_obstructed(p(9,6)))
    for trial in range(30):
        board.regenerate(); assert_route()
    check('30 generated maps enumerate all roads with ordered checkpoints and shortest route legs',True)
finally:
    actors.destroy_actor(board)
(root/'RoadOrderVerificationResults.json').write_text(json.dumps({'passed':checks,'count':len(checks)},indent=2))
unreal.log(f'GEM_ROAD_CHECKS: {len(checks)} passed')
