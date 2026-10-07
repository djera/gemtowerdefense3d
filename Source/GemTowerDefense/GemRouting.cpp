#include "GemPrototype.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Algo/Reverse.h"

namespace
{
const FIntPoint Directions[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)};
bool Inside(FIntPoint P,int32 Size) { return P.X>=0 && P.Y>=0 && P.X<Size && P.Y<Size; }
bool ShortestPath(FIntPoint Start,FIntPoint End,int32 Size,TFunctionRef<bool(FIntPoint)> Allowed,TArray<FIntPoint>& Result)
{
    Result.Reset();
    if(!Inside(Start,Size) || !Inside(End,Size) || !Allowed(Start) || !Allowed(End)) return false;
    TArray<int32> Previous; Previous.Init(-2,Size*Size);
    TArray<FIntPoint> Queue; Queue.Add(Start); Previous[Start.Y*Size+Start.X]=-1;
    for(int32 Head=0;Head<Queue.Num();++Head)
    {
        const FIntPoint P=Queue[Head];
        if(P==End)
        {
            for(int32 N=End.Y*Size+End.X;N>=0;N=Previous[N]) Result.Add(FIntPoint(N%Size,N/Size));
            Algo::Reverse(Result); return true;
        }
        for(const auto& D:Directions)
        {
            const FIntPoint N=P+D;
            if(!Inside(N,Size) || Previous[N.Y*Size+N.X]!=-2 || !Allowed(N)) continue;
            Previous[N.Y*Size+N.X]=P.Y*Size+P.X; Queue.Add(N);
        }
    }
    return false;
}
}

void AGemBoard::BuildRoadOrder()
{
    RoadOrder.Reset();
    TSet<FIntPoint> Roads,Required;
    for(const auto& T:Tiles)
    {
        if(T.Type!=EGroundType::Snow && T.Type!=EGroundType::Grass) Roads.Add(T.Cell);
        if(T.Type==EGroundType::Road || T.Type==EGroundType::Checkpoint || T.Type==EGroundType::Entry || T.Type==EGroundType::Exit) Required.Add(T.Cell);
    }
    TArray<FIntPoint> Anchors={EntryCell}; Anchors.Append(CheckpointOrder); Anchors.Add(ExitCell);
    TArray<TArray<FIntPoint>> Legs; TSet<FIntPoint> Spine;
    for(int32 I=1;I<Anchors.Num();++I)
    {
        TArray<FIntPoint> Leg;
        if(!ShortestPath(Anchors[I-1],Anchors[I],GridSize,[&](FIntPoint P){return Roads.Contains(P);},Leg))
            ShortestPath(Anchors[I-1],Anchors[I],GridSize,[](FIntPoint){return true;},Leg);
        for(const auto& P:Leg) Spine.Add(P);
        Legs.Add(MoveTemp(Leg));
    }
    TSet<FIntPoint> Seen;
    auto Emit=[&](FIntPoint P)
    {
        if(!Roads.Contains(P) || Seen.Contains(P)) return;
        Seen.Add(P);
        // Intersections connect the road network but are never required targets.
        if(!Required.Contains(P)) return;
        FGemRoadTile T; T.Cell=P; T.Index=RoadOrder.Num()+1;
        T.Checkpoint=CheckpointOrder.Find(P)+1; RoadOrder.Add(T);
    };
    // Traverse branches through intersections to find required road tiles on the
    // first encounter with their attachment to the checkpoint journey.
    TFunction<void(FIntPoint)> Branches;
    Branches=[&](FIntPoint P)
    {
        for(const auto& D:Directions)
        {
            const FIntPoint N=P+D;
            if(!Roads.Contains(N) || Seen.Contains(N) || Spine.Contains(N)) continue;
            Emit(N); Branches(N);
        }
    };
    Emit(EntryCell);
    for(int32 I=0;I<Legs.Num();++I) for(int32 J=0;J<Legs[I].Num();++J)
    {
        const FIntPoint P=Legs[I][J];
        // Passing through a future checkpoint doesn't advance its required order.
        const bool Reserved=CheckpointOrder.Contains(P) || P==ExitCell;
        if(P!=ExitCell && (!Reserved || (P==Anchors[I+1] && J==Legs[I].Num()-1))) Emit(P);
        Branches(P);
    }
    // A hand-authored map can contain disconnected road islands. They remain
    // required, reached through snow/grass, and are numbered before the exit.
    while(Seen.Num()+1<Roads.Num())
    {
        FIntPoint Next=FIntPoint::NoneValue; int32 Best=MAX_int32;
        const FIntPoint Last=RoadOrder.Last().Cell;
        // Scan in row order for deterministic ties (never TSet iteration order).
        for(int32 Y=0;Y<GridSize;++Y) for(int32 X=0;X<GridSize;++X)
        {
            const FIntPoint P(X,Y); const int32 Distance=FMath::Abs(X-Last.X)+FMath::Abs(Y-Last.Y);
            if(P!=ExitCell && Roads.Contains(P) && !Seen.Contains(P) && Distance<Best) { Next=P; Best=Distance; }
        }
        if(Next==FIntPoint::NoneValue) break;
        Emit(Next); Branches(Next);
    }
    Emit(ExitCell);
}

bool AGemBoard::RoadIsBlocked(FIntPoint Cell,const FIntPoint* ExtraBlock) const
{
    const FIntPoint Center=Cell*2;
    // Both road and gem occupy 2x2 half cells. A positive-area overlap, even
    // just one 1x1 quadrant, excludes the entire road tile. Edge touching doesn't.
    auto Overlaps=[&](FIntPoint P){return FMath::Abs(P.X-Center.X)<2 && FMath::Abs(P.Y-Center.Y)<2;};
    if(ExtraBlock && Overlaps(*ExtraBlock)) return true;
    return Occupied.ContainsByPredicate(Overlaps);
}
bool AGemBoard::RoadIsObstructed(FIntPoint Cell) const { return RoadIsBlocked(Cell,nullptr); }
int32 AGemBoard::RequiredRoadCount() const
{ int32 Count=0; for(const auto& T:RoadOrder) Count+=!T.bExcluded; return Count; }
int32 AGemBoard::EnemyRoadTarget(int32 I) const
{
    if(!Enemies.IsValidIndex(I)) return 0;
    const auto& Indices=Enemies[I].bFlying?AirRoadIndices:RouteRoadIndices;
    return Indices.IsValidIndex(Enemies[I].RouteIndex)?Indices[Enemies[I].RouteIndex]:0;
}
TArray<FIntPoint> AGemBoard::DebugRouteCells() const
{ TArray<FIntPoint> Cells; for(const auto& P:Route) Cells.Add(Snap(P)); return Cells; }

bool AGemBoard::FindRoute(TArray<FVector>& Result,const FIntPoint* ExtraBlock,TArray<int32>* RoadIndices) const
{
    Result.Reset(); if(RoadIndices) RoadIndices->Reset();
    TArray<FGemRoadTile> Goals;
    for(const auto& T:RoadOrder)
    {
        const bool Blocked=RoadIsBlocked(T.Cell,ExtraBlock);
        if(Blocked && (T.Checkpoint>0 || T.Cell==EntryCell || T.Cell==ExitCell)) return false;
        if(!Blocked) Goals.Add(T);
    }
    if(Goals.Num()<2 || Goals[0].Cell!=EntryCell || Goals.Last().Cell!=ExitCell) return false;
    constexpr int32 HalfSize=GridSize*2-1;
    TBitArray<> Blocked(false,HalfSize*HalfSize);
    auto AddBlock=[&](FIntPoint P)
    { for(int32 Y=P.Y-1;Y<=P.Y+1;++Y) for(int32 X=P.X-1;X<=P.X+1;++X) if(Inside(FIntPoint(X,Y),HalfSize)) Blocked[Y*HalfSize+X]=true; };
    for(const auto& P:Occupied) AddBlock(P);
    if(ExtraBlock) AddBlock(*ExtraBlock);
    for(int32 I=1;I<Goals.Num();++I)
    {
        TArray<FIntPoint> Leg;
        if(!ShortestPath(Goals[I-1].Cell*2,Goals[I].Cell*2,HalfSize,[&](FIntPoint P){return !Blocked[P.Y*HalfSize+P.X];},Leg))
        { Result.Reset(); if(RoadIndices) RoadIndices->Reset(); return false; }
        for(const auto& P:Leg)
        {
            const FVector Position=PlacementPosition(P)+FVector(0,0,18);
            if(!Result.IsEmpty() && Result.Last().Equals(Position)) continue;
            if(RoadIndices) RoadIndices->Add(Result.IsEmpty()?Goals[0].Index:Goals[I].Index);
            Result.Add(Position);
        }
    }
    return true;
}
bool AGemBoard::HasValidRoute() const { TArray<FVector> Test; return FindRoute(Test); }
void AGemBoard::RebuildRoute()
{
    for(auto& T:RoadOrder) T.bExcluded=RoadIsObstructed(T.Cell);
    FindRoute(Route,nullptr,&RouteRoadIndices); AirRoute.Reset(); AirRoadIndices.Reset();
    if(!Route.IsEmpty()) for(const auto& T:RoadOrder) if(!T.bExcluded)
    { AirRoute.Add(PlacementPosition(T.Cell*2)+FVector(0,0,78)); AirRoadIndices.Add(T.Index); }
    RouteMarkers->ClearInstances();
    RouteMarkers->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_Cel_Path.M_Cel_Path")));
    for(int32 I=0;I<Route.Num();I+=2) RouteMarkers->AddInstance(FTransform(FRotator::ZeroRotator,Route[I]-FVector(0,0,16),FVector(.07f)));
}
