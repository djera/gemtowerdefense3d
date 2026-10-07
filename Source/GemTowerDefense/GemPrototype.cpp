#include "GemPrototype.h"
#include "GemCameraHandler.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "EngineUtils.h"

static const TCHAR* GemNames[] = {TEXT("Amethyst"),TEXT("Aquamarine"),TEXT("Diamond"),TEXT("Emerald"),TEXT("Opal"),TEXT("Ruby"),TEXT("Sapphire"),TEXT("Topaz")};
static const TCHAR* QualityNames[] = {TEXT("Chipped"),TEXT("Flawed"),TEXT("Normal"),TEXT("Flawless"),TEXT("Perfect")};
static constexpr int32 QualityChances[9][5]={{100,0,0,0,0},{70,30,0,0,0},{60,30,10,0,0},{50,30,20,0,0},{40,30,20,10,0},{30,30,30,10,0},{20,30,30,20,0},{10,30,30,30,0},{0,30,30,30,10}};
static constexpr int32 UpgradeCosts[9]={20,50,80,110,140,170,200,230,0};
static bool IsCheckpoint(TCHAR Symbol) { return Symbol=='C' || (Symbol>='1' && Symbol<='9'); }

AGemBoard::AGemBoard()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    const TCHAR* GroundNames[] = {TEXT("Snow"),TEXT("Grass"),TEXT("Road"),TEXT("Intersection"),TEXT("Road"),TEXT("Road"),TEXT("Road")};
    for (int32 I=0;I<7;++I)
    {
        auto* Layer=CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Tiles_%d"),I));
        Layer->SetupAttachment(RootComponent);
        Layer->SetStaticMesh(Cube.Object);
        Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Layer->SetCastShadow(false);
        ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(*FString::Printf(TEXT("/Game/Prototype/Materials/M_%s.M_%s"),GroundNames[I],GroundNames[I]));
        Layer->SetMaterial(0,Mat.Object);
        Terrain.Add(Layer);
    }
    CheckpointMarkers=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CheckpointMarkers"));
    CheckpointMarkers->SetupAttachment(RootComponent);
    ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    ConstructorHelpers::FObjectFinder<UMaterialInterface> CheckpointMat(TEXT("/Game/Prototype/Materials/M_Checkpoint.M_Checkpoint"));
    CheckpointMarkers->SetStaticMesh(Cylinder.Object);
    CheckpointMarkers->SetMaterial(0,CheckpointMat.Object);
    CheckpointMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CheckpointMarkers->SetCastShadow(false);
    RouteMarkers=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RouteMarkers"));
    RouteMarkers->SetupAttachment(RootComponent);
    ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    RouteMarkers->SetStaticMesh(Sphere.Object);
    RouteMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RouteMarkers->SetCastShadow(false);
    for (const TCHAR* Name:GemNames)
    for (const TCHAR* Quality:QualityNames)
    {
        ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*FString::Printf(TEXT("/Game/Gems/Meshes/SM_%s_%s.SM_%s_%s"),Name,Quality,Name,Quality));
        GemModels.Add(Mesh.Object);
    }
    Ghost=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlacementPreview"));
    Ghost->SetupAttachment(RootComponent);
    Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Ghost->SetCastShadow(false);
    ConstructorHelpers::FObjectFinder<UMaterialInterface> Good(TEXT("/Game/Prototype/Materials/M_Valid.M_Valid"));
    ConstructorHelpers::FObjectFinder<UMaterialInterface> Bad(TEXT("/Game/Prototype/Materials/M_Invalid.M_Invalid"));
    ValidMaterial=Good.Object; InvalidMaterial=Bad.Object;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("IsometricCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->ProjectionMode=ECameraProjectionMode::Orthographic;
    Camera->SetRelativeRotation(FRotator(-35.264f,-45.f,0));
    Camera->SetRelativeLocation(-Camera->GetForwardVector()*3500.f);
    Camera->OrthoWidth=3600;
    Camera->bConstrainAspectRatio=false;
    Camera->bOverrideAspectRatioAxisConstraint=true;
    Camera->AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
    Camera->bAutoCalculateOrthoPlanes=false;
    Camera->bUpdateOrthoPlanes=false;
    Camera->OrthoNearClipPlane=1.f;
    Camera->OrthoFarClipPlane=10000.f;
    auto* Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    Sun->SetupAttachment(RootComponent);
    Sun->SetRelativeRotation(FRotator(-55,-25,0));
    Sun->SetIntensity(3.f);
    Sun->SetCastShadows(true);
}
void AGemBoard::PostLoad()
{
    Super::PostLoad();
    // Older saved boards contain five terrain layers. Rebind to the current
    // native components so new tile types also work when opening an old level.
    Terrain.Reset();
    for (int32 I=0;I<7;++I)
        Terrain.Add(CastChecked<UInstancedStaticMeshComponent>(GetDefaultSubobjectByName(*FString::Printf(TEXT("Tiles_%d"),I))));
    GemModels.Reset();
    for (const TCHAR* Name:GemNames) for (const TCHAR* Quality:QualityNames)
        GemModels.Add(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Gems/Meshes/SM_%s_%s.SM_%s_%s"),Name,Quality,Name,Quality)));
}
void AGemBoard::BeginPlay() { Super::BeginPlay(); BuildLandscape(); ResetRun(); Ghost->SetVisibility(false); }
void AGemBoard::CalcCamera(float DeltaTime,FMinimalViewInfo& OutResult)
{
    Camera->GetCameraView(DeltaTime,OutResult);
    OutResult.ProjectionMode=ECameraProjectionMode::Orthographic;
    OutResult.OrthoWidth=Camera->OrthoWidth;
    OutResult.Location=Camera->GetComponentLocation();
    OutResult.Rotation=Camera->GetComponentRotation();
    OutResult.AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
    OutResult.bAutoCalculateOrthoPlanes=false;
    OutResult.bUpdateOrthoPlanes=false;
    OutResult.OrthoNearClipPlane=1.f;
    OutResult.OrthoFarClipPlane=10000.f;
}
void AGemBoard::BuildLandscape()
{
    LoadDefinitions();
    const TCHAR* Names[]={TEXT("Snow"),TEXT("Grass"),TEXT("Road"),TEXT("Intersection"),TEXT("Road"),TEXT("Road"),TEXT("Road")};
    for (int32 I=0;I<Terrain.Num();++I)
        Terrain[I]->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Prototype/Materials/M_%s.M_%s"),Names[I],Names[I])));
    CheckpointMarkers->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_Checkpoint.M_Checkpoint")));
    for (const auto& Layer:Terrain) Layer->ClearInstances();
    CheckpointMarkers->ClearInstances();
    Tiles.Reset();
    // Embedded fallback keeps the same board available in packaged builds.
    const TCHAR* DefaultRows[]={
        TEXT("SSSSSSSSSSSSSSSSSSSS"),TEXT("SSSSSSSSSSSSSSSSSSSS"),
        TEXT("ERI1ISSSI5IRRRI4ISSS"),TEXT("SSSISSSSSISSSSSISSSS"),
        TEXT("SSSRSSSSSRSSSSSRSSSS"),TEXT("SSSRSSSSSRSSSSSRSSSS"),
        TEXT("SSSRSSSSSRSSSSSRSSSS"),TEXT("SSSRSSSSSRSSSSSRSSSS"),
        TEXT("SSSRSSSSSRSSSSSRSSSS"),TEXT("SSSISSSSSISSSSSISSSS"),
        TEXT("SSS2IRRRRRRRRRR3ISSS"),TEXT("SSSISSSSSRSSSSSISSSS"),
        TEXT("SSSSSSSSSRSSSSSSSSSS"),TEXT("SSSSSSSSSRSSSSSSSSSS"),
        TEXT("SSSSSSSSSRSSSSSSSSSS"),TEXT("SSSSSSSSSISSSSSSSSSS"),
        TEXT("SSSSSSSSI6IRRRRRRRRX"),TEXT("SSSSSSSSSSSSSSSSSSSS"),
        TEXT("SSSSSSSSSSSSSSSSSSSS"),TEXT("SSSSSSSSSSSSSSSSSSSS")};
    TArray<FString> Rows;
    const bool Loaded=LayoutRows.Num()>0 ? (Rows=LayoutRows,true) : FFileHelper::LoadFileToStringArray(Rows,*(FPaths::ProjectDir()/TEXT("Maps/BoardLayout.txt")));
    bool Valid=Loaded && Rows.Num()==GridSize;
    int32 Entries=0,Exits=0;
    int32 Numbers[10]={},LastNumber=0;
    for (const FString& Row:Rows)
    {
        if (Row.Len()!=GridSize) Valid=false;
        for (TCHAR Symbol:Row)
        {
            if (!FString(TEXT("SGRICEX123456789")).Contains(FString::Chr(Symbol))) Valid=false;
            Entries+=Symbol=='E'; Exits+=Symbol=='X';
            if (Symbol>='1' && Symbol<='9') { ++Numbers[Symbol-'0']; LastNumber=FMath::Max(LastNumber,Symbol-'0'); }
        }
    }
    if (Entries!=1 || Exits!=1) Valid=false;
    for (int32 N=1;N<=LastNumber;++N) if (Numbers[N]!=1) Valid=false;
    if (!Valid)
    {
        if (Loaded) UE_LOG(LogTemp,Warning,TEXT("Invalid BoardLayout.txt; using embedded 20x20 layout."));
        Rows.Reset(); for (const TCHAR* Row:DefaultRows) Rows.Add(Row);
    }
    LayoutRows=Rows;
    CheckpointOrder.Reset();
    for (TCHAR Number='1';Number<='9';++Number)
        for (int32 Y=0;Y<20;++Y) for (int32 X=0;X<20;++X) if (Rows[Y][X]==Number) CheckpointOrder.Add(FIntPoint(X,Y));
    if (CheckpointOrder.IsEmpty())
        for (int32 Y=0;Y<20;++Y) for (int32 X=0;X<20;++X) if (Rows[Y][X]=='C') CheckpointOrder.Add(FIntPoint(X,Y));
    for (int32 Y=0;Y<GridSize;++Y) for (int32 X=0;X<GridSize;++X)
    {
        EGroundType Type=EGroundType::Snow;
        if (FMath::PerlinNoise2D(FVector2D(X*.16f+LandscapeSeed*.013f,Y*.16f))>0) Type=EGroundType::Grass;
        switch (Rows[Y][X])
        {
            case 'G': Type=EGroundType::Grass; break;
            case 'R': Type=EGroundType::Road; break;
            case 'I': Type=EGroundType::Intersection; break;
            case 'C': Type=EGroundType::Checkpoint; break;
            case 'E': Type=EGroundType::Entry; EntryCell=FIntPoint(X,Y); break;
            case 'X': Type=EGroundType::Exit; ExitCell=FIntPoint(X,Y); break;
        }
        if (IsCheckpoint(Rows[Y][X])) Type=EGroundType::Checkpoint;
        FGroundTile Tile; Tile.Cell=FIntPoint(X,Y); Tile.Type=Type; Tiles.Add(Tile);
        Terrain[static_cast<int32>(Type)]->AddInstance(FTransform(FRotator::ZeroRotator,FVector((X-9.5f)*CellSize,(Y-9.5f)*CellSize,0),FVector(.975f,.975f,.16f)));
        if (Type==EGroundType::Checkpoint)
            CheckpointMarkers->AddInstance(FTransform(FRotator::ZeroRotator,FVector((X-9.5f)*CellSize,(Y-9.5f)*CellSize,9.5f),FVector(.48f,.48f,.025f)));
    }
    RebuildRoute();
}
FIntPoint AGemBoard::Snap(FVector World) const
{
    return FIntPoint(FMath::RoundToInt((World.X+950.f)/50.f),FMath::RoundToInt((World.Y+950.f)/50.f));
}
FVector AGemBoard::PlacementPosition(FIntPoint P) const { return FVector(-950.f+P.X*50.f,-950.f+P.Y*50.f,10.f); }
bool AGemBoard::FootprintAvailable(FIntPoint P) const
{
    if (P.X<0 || P.Y<0 || P.X>38 || P.Y>38) return false;
    // Full 100x100 footprint. Touching edges are allowed; positive overlap is not.
    for (FIntPoint Other:Occupied) if (FMath::Abs(P.X-Other.X)<2 && FMath::Abs(P.Y-Other.Y)<2) return false;
    for (const FGroundTile& Tile:Tiles)
        if ((Tile.Type==EGroundType::Checkpoint || Tile.Type==EGroundType::Intersection || Tile.Type==EGroundType::Entry || Tile.Type==EGroundType::Exit) && FMath::Abs(P.X-Tile.Cell.X*2)<2 && FMath::Abs(P.Y-Tile.Cell.Y*2)<2) return false;
    return true;
}
bool AGemBoard::CanPlace(FIntPoint P) const
{
    if (!FootprintAvailable(P)) return false;
    TArray<FVector> TestRoute;
    return FindRoute(TestRoute,&P);
}
void AGemBoard::ApplyGemSize(UStaticMeshComponent* Mesh) const
{
    if (!Mesh->GetStaticMesh()) return;
    const FVector Size=Mesh->GetStaticMesh()->GetBoundingBox().GetSize();
    // Same square footprint for every gem; one uniform scale preserves proportions.
    const float Scale=90.f/FMath::Max(Size.X,Size.Y);
    Mesh->SetWorldScale3D(FVector(Scale));
}
void AGemBoard::Place(FIntPoint P,int32 Type,int32 Quality)
{
    if (Phase!=ERoundPhase::Placing || OffersPlaced>=5 || !CanPlace(P) || Type<0 || Type>=8 || Quality<0 || Quality>=5) return;
    const int32 Model=Type*5+Quality;
    if (!GemModels.IsValidIndex(Model) || !GemModels[Model]) return;
    auto* Mesh=NewObject<UStaticMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent); Mesh->SetStaticMesh(GemModels[Model]);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
    Mesh->SetWorldLocation(PlacementPosition(P)); ApplyGemSize(Mesh);
    Occupied.Add(P); Placed.Add(Mesh);
    FGemPiece Piece; Piece.Type=Type; Piece.Quality=Quality;
    Pieces.Add(Piece); AttackCooldown.Add(0); ++OffersPlaced;
    Selected=Pieces.Num()-1;
    SelectedEnemy=INDEX_NONE;
    if (OffersPlaced==5) { Phase=ERoundPhase::Choosing; Selected=INDEX_NONE; }
    RebuildRoute();
}
void AGemBoard::Remove(FIntPoint P)
{
    for (int32 I=Occupied.Num()-1;I>=0;--I)
        if (FMath::Abs(P.X-Occupied[I].X)<=1 && FMath::Abs(P.Y-Occupied[I].Y)<=1)
        { Placed[I]->DestroyComponent(); Placed.RemoveAt(I); Occupied.RemoveAt(I); Pieces.RemoveAt(I); AttackCooldown.RemoveAt(I); Selected=INDEX_NONE; RebuildRoute(); return; }
}
void AGemBoard::ClearGems()
{
    for (const auto& Mesh:Placed) Mesh->DestroyComponent();
    Placed.Reset(); Occupied.Reset();
    Pieces.Reset(); AttackCooldown.Reset(); Selected=INDEX_NONE; SelectedEnemy=INDEX_NONE;
}
void AGemBoard::LoadDefaultLayout()
{
    ClearGems(); LayoutRows.Reset(); CheckpointOrder.Reset(); BuildLandscape(); ResetRun();
}
void AGemBoard::Regenerate()
{
    ClearGems();
    FRandomStream Random(++LandscapeSeed);
    LayoutRows.Init(FString::ChrN(GridSize,'S'),GridSize);
    TArray<FIntPoint> Centers;
    constexpr int32 CheckpointCount=6;
    for (int32 Attempt=0;Attempt<50 && Centers.Num()<CheckpointCount;++Attempt)
    {
        Centers.Reset();
        for (int32 Trial=0;Trial<1000 && Centers.Num()<CheckpointCount;++Trial)
        {
            FIntPoint P(Random.RandRange(2,17),Random.RandRange(2,17));
            bool Fits=true;
            for (FIntPoint C:Centers) if (FMath::Abs(P.X-C.X)+FMath::Abs(P.Y-C.Y)<6) Fits=false;
            if (Fits) Centers.Add(P);
        }
    }
    if (Centers.Num()!=CheckpointCount) Centers={FIntPoint(3,3),FIntPoint(15,3),FIntPoint(9,8),FIntPoint(3,13),FIntPoint(15,13),FIntPoint(9,17)};
    const FIntPoint Directions[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)};
    TArray<int32> CheckpointOwner; CheckpointOwner.Init(-1,400);
    TArray<bool> Closed; Closed.Init(false,400);
    TArray<TArray<int32>> Ports; Ports.SetNum(CheckpointCount);
    auto Id=[](FIntPoint P) { return P.Y*GridSize+P.X; };
    for (int32 I=0;I<CheckpointCount;++I)
    {
        const FIntPoint C=Centers[I]; LayoutRows[C.Y][C.X]='1'+I; CheckpointOwner[Id(C)]=I;
        const int32 Missing=Random.RandRange(0,3);
        for (int32 D=0;D<4;++D)
        {
            const FIntPoint P=C+Directions[D];
            if (D==Missing) { Closed[Id(P)]=true; continue; }
            LayoutRows[P.Y][P.X]='I'; CheckpointOwner[Id(P)]=I; Ports[I].Add(Id(P));
        }
    }
    // Multi-source BFS joins each T to the existing network without overwriting
    // checkpoint definitions or opening the fourth side of a checkpoint.
    TArray<bool> Network; Network.Init(false,400);
    for (int32 P:Ports[0]) Network[P]=true;
    Network[Id(Centers[0])]=true;
    for (int32 I=1;I<CheckpointCount;++I)
    {
        TArray<int32> Previous; Previous.Init(-2,400);
        TArray<int32> Queue;
        for (int32 P:Ports[I]) { Previous[P]=-1; Queue.Add(P); }
        int32 Found=-1;
        for (int32 Head=0;Head<Queue.Num() && Found<0;++Head)
        {
            const int32 Here=Queue[Head];
            for (FIntPoint D:Directions)
            {
                const FIntPoint Next(Here%20+D.X,Here/20+D.Y);
                if (Next.X<0 || Next.Y<0 || Next.X>=20 || Next.Y>=20) continue;
                const int32 N=Id(Next);
                if (Previous[N]!=-2 || Closed[N] || IsCheckpoint(LayoutRows[Next.Y][Next.X])) continue;
                if (CheckpointOwner[N]>=I && CheckpointOwner[N]!=I) continue;
                Previous[N]=Here;
                if (Network[N]) { Found=N; break; }
                Queue.Add(N);
            }
        }
        checkf(Found>=0,TEXT("Checkpoint road generation must find a connected route"));
        for (int32 P=Found;P>=0;P=Previous[P])
        {
            if (LayoutRows[P/20][P%20]=='S') LayoutRows[P/20][P%20]='R';
            Network[P]=true;
        }
        for (int32 P:Ports[I]) Network[P]=true;
        Network[Id(Centers[I])]=true;
    }
    // Pick distinct boundary tiles, then join them to the first and last T.
    // Preserve each T's closed fourth side and all four checkpoint tiles.
    TArray<FIntPoint> Edge;
    for (int32 Y=0;Y<GridSize;++Y) for (int32 X=0;X<GridSize;++X)
        if (X==0 || Y==0 || X==GridSize-1 || Y==GridSize-1) Edge.Add(FIntPoint(X,Y));
    const FIntPoint Entry=Edge[Random.RandRange(0,Edge.Num()-1)];
    TArray<FIntPoint> ExitOptions;
    for (FIntPoint P:Edge) if (FMath::Abs(P.X-Entry.X)+FMath::Abs(P.Y-Entry.Y)>=10) ExitOptions.Add(P);
    const FIntPoint Exit=ExitOptions[Random.RandRange(0,ExitOptions.Num()-1)];
    auto ConnectEdge=[&](FIntPoint Endpoint,int32 Checkpoint,TCHAR Symbol)
    {
        TArray<int32> Previous; Previous.Init(-2,400);
        TArray<int32> Queue; Queue.Add(Id(Endpoint)); Previous[Id(Endpoint)]=-1;
        int32 Found=-1;
        for (int32 Head=0;Head<Queue.Num();++Head)
        {
            const int32 Here=Queue[Head];
            if (Ports[Checkpoint].Contains(Here)) { Found=Here; break; }
            for (FIntPoint D:Directions)
            {
                const FIntPoint Next(Here%20+D.X,Here/20+D.Y);
                if (Next.X<0 || Next.Y<0 || Next.X>=20 || Next.Y>=20) continue;
                const int32 N=Id(Next);
                if (Previous[N]!=-2 || Closed[N] || IsCheckpoint(LayoutRows[Next.Y][Next.X])) continue;
                Previous[N]=Here; Queue.Add(N);
            }
        }
        checkf(Found>=0,TEXT("Boundary endpoint must connect to its checkpoint"));
        for (int32 P=Found;P>=0;P=Previous[P])
            if (LayoutRows[P/20][P%20]=='S') LayoutRows[P/20][P%20]='R';
        LayoutRows[Endpoint.Y][Endpoint.X]=Symbol;
    };
    ConnectEdge(Entry,0,'E'); ConnectEdge(Exit,CheckpointCount-1,'X');
    CheckpointOrder=Centers;
    BuildLandscape(); ResetRun();
}
void AGemBoard::UpdateGhost(FIntPoint P,int32 Type,bool Visible)
{
    Ghost->SetVisibility(Visible);
    if (!Visible || !GemModels.IsValidIndex(Type)) return;
    Ghost->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Ghost->SetWorldLocation(PlacementPosition(P)+FVector(0,0,1.5f));
    Ghost->SetMaterial(0,CanPlace(P)?ValidMaterial:InvalidMaterial);
    Ghost->SetWorldScale3D(FVector(.98f,.98f,.025f));
}
int32 AGemController::UpgradeCost() const { return UpgradeCosts[FMath::Clamp(ChanceLevel,0,8)]; }
int32 AGemController::QualityChance(int32 Quality) const { return Quality>=0 && Quality<5 ? QualityChances[FMath::Clamp(ChanceLevel,0,8)][Quality] : 0; }
int32 AGemController::RollQuality()
{
    const int32 Roll=FMath::RandRange(1,100);
    int32 Cumulative=0;
    for (int32 Quality=0;Quality<5;++Quality) { Cumulative+=QualityChance(Quality); if (Roll<=Cumulative) return Quality; }
    return 4;
}
void AGemController::UpgradeChance()
{
    if (ChanceLevel>=8) { Status=TEXT("Chance level is already at maximum"); return; }
    if (Gold<UpgradeCost()) { Status=TEXT("Not enough gold to upgrade chance"); return; }
    Gold-=UpgradeCost(); ++ChanceLevel;
    Status=FString::Printf(TEXT("Chance upgraded to level %d"),ChanceLevel);
}
AGemController::AGemController()
{
    bShowMouseCursor=true; bAutoManageActiveCameraTarget=false; DefaultMouseCursor=EMouseCursor::Crosshairs;
    CameraHandler=CreateDefaultSubobject<UGemCameraHandler>(TEXT("CameraHandler"));
}
void AGemController::ResetCamera()
{
    CameraHandler->ResetView(); bCameraDragging=false; bHasPreviousMouse=false;
    Status=TEXT("Camera reset to the starting isometric view");
}
void AGemController::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AGemBoard> It(GetWorld());It;++It) { Board=*It; break; }
    if (!Board) Board=GetWorld()->SpawnActor<AGemBoard>();
    SetViewTarget(Board);
    FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode);
    if (FParse::Param(FCommandLine::Get(),TEXT("GemSmokeTest")))
    {
        int32 Passed=0,Failed=0;
        auto Check=[&](bool Success,const TCHAR* Name)
        {
            if (Success) { ++Passed; UE_LOG(LogTemp,Display,TEXT("GEM_TEST_PASS: %s"),Name); }
            else { ++Failed; UE_LOG(LogTemp,Error,TEXT("GEM_TEST_FAIL: %s"),Name); }
        };
        TArray<int32> Rejected;
        bool RouteCheck=true;
        for (int32 Y=0;Y<=38;Y+=2)
        {
            Board->OffersPlaced=0; Board->Phase=ERoundPhase::Placing;
            const int32 Before=Board->GemCount();
            const bool Allowed=Board->CanPlace(FIntPoint(14,Y));
            if (!Allowed) Rejected.Add(Y);
            Board->Place(FIntPoint(14,Y),0,0);
            RouteCheck&=Board->GemCount()==Before+(Allowed?1:0) && Board->HasValidRoute();
        }
        Check(RouteCheck && Rejected.Num()==1 && Rejected[0]==38,TEXT("Closing a wall across E-checkpoints-X is rejected"));
        Board->ResetRun();
        auto OfferFive=[&]() { for (int32 I=0;I<5;++I) Board->Place(FIntPoint(8+I*4,14),0,0); };
        OfferFive();
        Check(Board->Phase==ERoundPhase::Choosing && Board->GemCount()==5 && !Board->CanKeepSelected(),TEXT("Five offers switch to selection and require an explicit gem selection"));
        Board->Place(FIntPoint(28,14),1,0);
        Check(Board->GemCount()==5,TEXT("A sixth offer cannot be placed"));
        Board->SelectAt(FIntPoint(8,14));
        Check(Board->CanKeepSelected(),TEXT("Selecting an offered gem enables Place"));
        Check(Board->ClearAllGems() && Board->GemCount()==0 && Board->OffersPlaced==0 && Board->Phase==ERoundPhase::Placing,TEXT("Clear Gems restarts placement"));
        OfferFive(); Board->SelectAt(FIntPoint(8,14));
        Check(Board->KeepSelected(),TEXT("Place starts the wave"));
        int32 Rocks=0,Towers=0;
        for (const auto& Piece:Board->Pieces) { Rocks+=Piece.bRock; Towers+=!Piece.bRock && !Piece.bPending; }
        Check(Rocks==4 && Towers==1,TEXT("Only the chosen gem stays; four become stones"));
        Check(!Board->ClearAllGems(),TEXT("Clear Gems cannot erase the maze during combat"));
        Board->Tick(.001f);
        Check(Board->Enemies.Num()==1 && FVector::Dist2D(Board->Enemies[0].Mesh->GetComponentLocation(),Board->PlacementPosition(Board->EntryCell*2))<1,TEXT("Enemies spawn at the E tile"));
        Board->SelectEnemy(0);
        Check(Board->Selected==INDEX_NONE && Board->SelectedEnemy==0 && Board->SelectedInfo().Num()>=7,TEXT("Enemy selection exposes its live stats"));
        if (!Board->Enemies.IsEmpty()) Board->Enemies[0].RouteIndex=100000;
        Board->Tick(.001f);
        Check(Board->Lives==19 && Board->SelectedEnemy==INDEX_NONE,TEXT("An escaping enemy removes one life and clears its selection"));
        Board->ResetRun(); OfferFive(); Board->SelectAt(FIntPoint(8,14));
        Check(Board->MergeSelected(2) && Board->Pieces[0].Quality==1,TEXT("Two matching gems merge one grade higher"));
        Board->ResetRun(); OfferFive(); Board->SelectAt(FIntPoint(8,14));
        Check(Board->MergeSelected(4) && Board->Pieces[0].Quality==2,TEXT("Four matching gems merge two grades higher"));
        UE_LOG(LogTemp,Display,TEXT("GEM_SMOKE_RESULTS: %d passed / %d failed"),Passed,Failed);
        Board->ResetRun();
        for (int32 I=0;I<5;++I) Board->Place(FIntPoint(8+I*4,14),I,I);
        Board->SelectAt(FIntPoint(24,14)); bShowSelection=true;
        FTimerHandle CaptureTimer,ExitTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer,FTimerDelegate::CreateLambda([this]()
        {
            int32 Width,Height; GetViewportSize(Width,Height);
            UE_LOG(LogTemp,Display,TEXT("GEM_CAMERA viewport=%dx%d width=%.1f location=%s target=%s"),Width,Height,Board->Camera->OrthoWidth,*Board->Camera->GetComponentLocation().ToString(),*GetNameSafe(GetViewTarget()));
            UE_LOG(LogTemp,Display,TEXT("GEM_POV ortho=%d width=%.1f location=%s"),PlayerCameraManager->IsOrthographic(),PlayerCameraManager->GetOrthoWidth(),*PlayerCameraManager->GetCameraLocation().ToString());
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("PrototypePreview.png"),true,false);
        }),8.f,false);
        FTimerHandle PlaceTimer,CombatCapture,OrbitTimer,OrbitCapture;
        GetWorldTimerManager().SetTimer(PlaceTimer,FTimerDelegate::CreateLambda([this]() { Board->KeepSelected(); }),9.f,false);
        GetWorldTimerManager().SetTimer(CombatCapture,FTimerDelegate::CreateLambda([this]()
        { if (!Board->Enemies.IsEmpty()) Board->SelectEnemy(0); FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("CombatPreview.png"),true,false); }),12.f,false);
        GetWorldTimerManager().SetTimer(OrbitTimer,FTimerDelegate::CreateLambda([this]() { CameraHandler->Orbit(FVector2D(250,-65)); CameraHandler->Pan(FVector2D(60,15),Board->Camera->OrthoWidth/1440); }),13.f,false);
        GetWorldTimerManager().SetTimer(OrbitCapture,FTimerDelegate::CreateLambda([this]() { FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("CameraPreview.png"),true,false); }),15.f,false);
        GetWorldTimerManager().SetTimer(ExitTimer,FTimerDelegate::CreateLambda([this]()
        { ConsoleCommand(TEXT("quit")); }),17.f,false);
    }
}
void AGemController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!Board) return;
    if (GetViewTarget()!=Board) SetViewTarget(Board);
    int32 W,H; GetViewportSize(W,H); if (W<=0 || H<=0) return;
    const float Side=SidebarWidth(W),Header=64.f;
    float X=0,Y=0; const bool HasMouse=GetMousePosition(X,Y);
    const bool InBoard=HasMouse && X<W-Side && Y>Header && Y<H-100;
    const bool RightHeld=IsInputKeyDown(EKeys::RightMouseButton),MiddleHeld=IsInputKeyDown(EKeys::MiddleMouseButton);
    if (InBoard && (WasInputKeyJustPressed(EKeys::RightMouseButton) || WasInputKeyJustPressed(EKeys::MiddleMouseButton))) bCameraDragging=true;
    if (!RightHeld && !MiddleHeld) bCameraDragging=false;
    if (bCameraDragging && HasMouse && bHasPreviousMouse)
    {
        const FVector2D Delta=FVector2D(X,Y)-PreviousMouse;
        if (RightHeld) CameraHandler->Orbit(Delta);
        else if (MiddleHeld) CameraHandler->Pan(Delta,Board->Camera->OrthoWidth/W);
    }
    PreviousMouse=FVector2D(X,Y); bHasPreviousMouse=HasMouse;
    const bool Click=WasInputKeyJustPressed(EKeys::LeftMouseButton) && !bCameraDragging;
    if (WasInputKeyJustPressed(EKeys::Home) || (Click && HasMouse && X>W-Side+24 && X<W-24 && Y>=782 && Y<=820)) ResetCamera();
    CameraHandler->Apply(Board->Camera,W,H,Side);
    PlayerCameraManager->bIsOrthographic=true;
    PlayerCameraManager->bAutoCalculateOrthoPlanes=false;
    PlayerCameraManager->bUpdateOrthoPlanes=false;
    PlayerCameraManager->SetOrthoWidth(Board->Camera->OrthoWidth);
    if (WasInputKeyJustPressed(EKeys::R)) { Board->Regenerate(); Status=TEXT("Generated 6 connected T-shaped checkpoints"); bShowSelection=false; }
    if (WasInputKeyJustPressed(EKeys::D)) { Board->LoadDefaultLayout(); Status=TEXT("Default layout restored"); }
    bHover=false;
    if (HasMouse && Click && X>W-Side+24 && X<W-24 && Y>=458 && Y<=500)
    { Board->Regenerate(); Status=TEXT("Generated 6 connected T-shaped checkpoints"); bShowSelection=false; }
    if (HasMouse && Click && X>W-Side+24 && X<W-24 && Y>=700 && Y<=742)
    { UpgradeChance(); }
    if (WasInputKeyJustPressed(EKeys::U)) UpgradeChance();
    if (HasMouse && Click && X>W-Side+24 && X<W-24 && Y>=88 && Y<=120)
    { bShowSelection=X>W-Side+Side*.5f; InfoScroll=0; }
    if (HasMouse && X>W-Side && bShowSelection)
    {
        if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) InfoScroll=FMath::Max(0,InfoScroll-2);
        if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) InfoScroll=FMath::Min(FMath::Max(0,Board->SelectedInfo().Num()-14),InfoScroll+2);
    }
    int32 Action=INDEX_NONE;
    const float ButtonWidth=(W-Side-96)/7;
    if (HasMouse && Click && X>=24 && X<W-Side-24 && Y>=H-80 && Y<=H-44)
    {
        const int32 Index=int32((X-24)/(ButtonWidth+8));
        if (Index<7 && X<=24+Index*(ButtonWidth+8)+ButtonWidth) Action=Index;
    }
    if (Action==0 || WasInputKeyJustPressed(EKeys::P)) Status=Board->KeepSelected()?TEXT("Gem built. The other four offers became stones. Wave started."):TEXT("Place five gems, then select one and press Place.");
    if (Action==1 || WasInputKeyJustPressed(EKeys::C)) Status=Board->ClearAllGems()?TEXT("All gems and stones cleared. Place five new gems for this round."):TEXT("Clear Gems is available during the build phase.");
    if (Action==2 || WasInputKeyJustPressed(EKeys::Two)) Status=Board->MergeSelected(2)?TEXT("Merged two gems. Wave started."):TEXT("Select a gem with a matching type and grade after placing five.");
    if (Action==3 || WasInputKeyJustPressed(EKeys::Four)) Status=Board->MergeSelected(4)?TEXT("Merged four gems. Wave started."):TEXT("Four matching gems are required; maximum grade is Perfect.");
    if (Action==4 || WasInputKeyJustPressed(EKeys::T)) bRecipesOpen=!bRecipesOpen;
    if (Action==5 || WasInputKeyJustPressed(EKeys::V)) Status=Board->UpgradeSelectedTower()?TEXT("Special tower upgraded"):TEXT("Select an upgradeable special tower during the build phase.");
    if (Action==6 || WasInputKeyJustPressed(EKeys::X)) Status=Board->DemolishSelectedRock()?TEXT("Stone removed"):TEXT("Select a stone during the build phase to remove it.");
    if (bRecipesOpen && HasMouse && Click && X>=24 && X<W-Side-24 && Y>=156 && Y<H-140)
    {
        const float RowHeight=FMath::Min(42.f,(H-300.f)/FMath::Max(1,Board->RecipeCount()));
        const int32 Recipe=int32((Y-156)/RowHeight);
        if (Recipe<Board->RecipeCount())
        {
            if (Board->CraftSelected(Recipe)) { Status=TEXT("Special tower built. Wave started."); bRecipesOpen=false; }
            else Status=TEXT("Select a recipe ingredient after placing all five gems. All ingredients must be on the board.");
        }
    }
    if (InBoard && !bCameraDragging && !bRecipesOpen)
    {
        FVector Origin,Direction;
        if (DeprojectScreenPositionToWorld(X,Y,Origin,Direction) && FMath::Abs(Direction.Z)>.001f)
        {
            const float T=(10.f-Origin.Z)/Direction.Z;
            Hover=Board->Snap(Origin+Direction*T);
            bHover=T>0 && Hover.X>=0 && Hover.Y>=0 && Hover.X<=38 && Hover.Y<=38;
            if (Click && Board->SelectRay(Origin,Direction))
            { bShowSelection=true; InfoScroll=0; Status=Board->SelectedDescription(); }
            else if (bHover && Click)
            {
                if (Board->PieceAt(Hover)!=INDEX_NONE)
                {
                    Board->SelectAt(Hover); bShowSelection=true; InfoScroll=0; Status=Board->SelectedDescription();
                }
                else if (Board->Phase!=ERoundPhase::Placing) Status=TEXT("Choose a gem to keep, or wait for the current wave to finish.");
                else if (Board->CanPlace(Hover))
                {
                    const int32 Type=FMath::RandRange(0,7),Quality=RollQuality();
                    Board->Place(Hover,Type,Quality);
                    Status=Board->Phase==ERoundPhase::Choosing?TEXT("Five gems offered. Select one, then press Place."):FString::Printf(TEXT("Placed %s %s"),QualityNames[Quality],GemNames[Type]);
                }
                else Status=TEXT("Blocked: overlap, protected tile, or no route from entry to exit");
            }
        }
    }
    Board->UpdateGhost(Hover,0,bHover && Board->Phase==ERoundPhase::Placing && Board->PieceAt(Hover)==INDEX_NONE);
}
void AGemHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* PC=Cast<AGemController>(GetOwningPlayerController()); if (!Canvas || !PC) return;
    const float W=Canvas->SizeX,H=Canvas->SizeY,Side=AGemController::SidebarWidth(W),Left=W-Side;
    const FLinearColor Ink(.80f,.86f,.93f),Muted(.42f,.52f,.63f),Accent(.27f,.85f,.73f);
    DrawRect(FLinearColor(.018f,.027f,.045f),0,0,W,64);
    DrawRect(FLinearColor(.025f,.038f,.060f),Left,64,Side,H-64);
    DrawRect(FLinearColor(.12f,.21f,.27f),Left,64,1,H-64);
    auto Label=[&](FString S,float X,float Y,FLinearColor Color,float Scale=1.f) { DrawText(S,Color,X,Y,GEngine->GetMediumFont(),Scale); };
    Label(TEXT("GEM / TOWER DEFENSE"),24,21,Accent,1.1f);
    Label(FString(TEXT("GOLD  "))+FText::AsNumber(PC->Gold).ToString(),W*.20f,23,FLinearColor(1,.76f,.28f),.9f);
    if (PC->Board) Label(FString::Printf(TEXT("SCORE  %d   |   WAVE  %d   |   LIVES  %d"),PC->Board->Score,PC->Board->Wave,PC->Board->Lives),W*.43f,23,Ink,.85f);
    Label(FString::Printf(TEXT("CHANCE LEVEL  %d / 8"),PC->ChanceLevel),W*.76f,23,Accent,.85f);
    const float TabWidth=(Side-48)/2;
    DrawRect(!PC->bShowSelection?FLinearColor(.1f,.25f,.3f):FLinearColor(.04f,.09f,.14f),Left+24,88,TabWidth,32);
    DrawRect(PC->bShowSelection?FLinearColor(.1f,.25f,.3f):FLinearColor(.04f,.09f,.14f),Left+24+TabWidth,88,TabWidth,32);
    Label(TEXT("MAP / 20 x 20"),Left+34,97,Accent,.8f);
    Label(TEXT("SELECTED"),Left+34+TabWidth,97,Accent,.8f);
    if (PC->Board && PC->bShowSelection)
    {
        const auto Lines=PC->Board->SelectedInfo();
        PC->InfoScroll=FMath::Clamp(PC->InfoScroll,0,FMath::Max(0,Lines.Num()-14));
        for (int32 I=PC->InfoScroll;I<Lines.Num() && I<PC->InfoScroll+14;++I)
            Label(Lines[I],Left+24,148+(I-PC->InfoScroll)*20,I==0?Accent:Ink,.76f);
        if (Lines.Num()>14) Label(TEXT("Mouse wheel to scroll details"),Left+24,435,Muted,.72f);
    }
    else if (PC->Board)
    {
        const float Step=(Side-48)/20.f;
        for (int32 Y=0;Y<PC->Board->LayoutRows.Num();++Y)
            for (int32 X=0;X<PC->Board->LayoutRows[Y].Len();++X)
            {
                const TCHAR C=PC->Board->LayoutRows[Y][X];
                FLinearColor Color=C=='E'?Accent:C=='X'?FLinearColor(1,.4f,.3f):IsCheckpoint(C)?FLinearColor(1,.72f,.2f):C=='I'?Accent:C=='R'?Ink:Muted;
                Label(FString::Chr(C),Left+24+X*Step,150+Y*12.f,Color,.72f);
            }
    }
    if (!PC->bShowSelection)
    {
        Label(TEXT("S  Snow / grass     R  Road"),Left+24,422,Ink,.75f);
        Label(TEXT("I  Intersection   1-6  Checkpoint order"),Left+24,439,Ink,.72f);
        Label(TEXT("E  Entry     X  Exit  /  road tiles"),Left+24,405,Ink,.75f);
    }
    float MouseX,MouseY; const bool Hot=PC->GetMousePosition(MouseX,MouseY) && MouseX>Left+24 && MouseX<W-24 && MouseY>=458 && MouseY<=500;
    DrawRect(Hot?FLinearColor(.12f,.38f,.35f):FLinearColor(.07f,.25f,.24f),Left+24,458,Side-48,42);
    Label(TEXT("REGENERATE  /  6 CHECKPOINTS"),Left+36,471,Accent,.8f);
    Label(TEXT("R  Regenerate     D  Restore default"),Left+24,515,Muted,.75f);
    Label(FString::Printf(TEXT("GEM CHANCE / LEVEL %d"),PC->ChanceLevel),Left+24,551,Accent,.95f);
    for (int32 Q=0;Q<5;++Q)
    {
        Label(QualityNames[Q],Left+24,584+Q*20,Ink,.85f);
        Label(FString::Printf(TEXT("%d%%"),PC->QualityChance(Q)),W-72,584+Q*20,PC->QualityChance(Q)>0?Accent:Muted,.85f);
    }
    const bool UpgradeHot=PC->GetMousePosition(MouseX,MouseY) && MouseX>Left+24 && MouseX<W-24 && MouseY>=700 && MouseY<=742;
    DrawRect(PC->ChanceLevel>=8?FLinearColor(.08f,.10f,.12f):UpgradeHot?FLinearColor(.38f,.30f,.12f):FLinearColor(.25f,.20f,.08f),Left+24,700,Side-48,42);
    Label(PC->ChanceLevel>=8?TEXT("MAXIMUM CHANCE LEVEL"):FString::Printf(TEXT("UPGRADE / %d GOLD"),PC->UpgradeCost()),Left+36,714,FLinearColor(1,.76f,.28f),.85f);
    Label(TEXT("U  Upgrade    |    Random gem type"),Left+24,754,Muted,.75f);
    const bool ResetHot=PC->GetMousePosition(MouseX,MouseY) && MouseX>Left+24 && MouseX<W-24 && MouseY>=782 && MouseY<=820;
    DrawRect(ResetHot?FLinearColor(.12f,.30f,.4f):FLinearColor(.07f,.17f,.25f),Left+24,782,Side-48,38);
    Label(TEXT("RESET CAMERA / HOME"),Left+36,793,Accent,.85f);
    Label(TEXT("Right drag: rotate   Middle drag: pan"),Left+24,836,Muted,.75f);
    if (PC->Board)
    {
        auto* Board=PC->Board.Get();
        FString Phase;
        switch (Board->Phase)
        {
            case ERoundPhase::Placing: Phase=FString::Printf(TEXT("BUILD / Place %d more random gems"),5-Board->OffersPlaced); break;
            case ERoundPhase::Choosing: Phase=TEXT("CHOOSE / Select one gem and press Place, merge or combine"); break;
            case ERoundPhase::Combat: Phase=TEXT("WAVE / Enemies travel from E through the checkpoints to X"); break;
            case ERoundPhase::Victory: Phase=TEXT("VICTORY / All waves cleared. D starts a new run."); break;
            case ERoundPhase::Defeat: Phase=TEXT("DEFEAT / No lives remaining. D starts a new run."); break;
        }
        Label(Phase,24,80,Accent,.95f);
        for (int32 I=0;I<2;++I)
        {
            FVector2D Screen;
            if (PC->ProjectWorldLocationToScreen(Board->PlacementPosition((I==0?Board->EntryCell:Board->ExitCell)*2)+FVector(0,0,30),Screen)
                && Screen.X>14 && Screen.X<Left-20 && Screen.Y>115 && Screen.Y<H-130)
            {
                DrawRect(FLinearColor(.02f,.035f,.05f,.9f),Screen.X-10,Screen.Y-10,22,22);
                Label(I==0?TEXT("E"):TEXT("X"),Screen.X-5,Screen.Y-8,I==0?Accent:FLinearColor(1,.4f,.3f),.85f);
            }
        }
        for (int32 I=0;I<Board->CheckpointOrder.Num();++I)
        {
            FVector2D Screen;
            if (PC->ProjectWorldLocationToScreen(Board->PlacementPosition(Board->CheckpointOrder[I]*2)+FVector(0,0,16),Screen)
                && Screen.X>14 && Screen.X<Left-20 && Screen.Y>115 && Screen.Y<H-130)
                Label(FString::FromInt(I+1),Screen.X-4,Screen.Y-7,FLinearColor(.05f,.045f,.02f),.8f);
        }
        for (const auto& Enemy:Board->Enemies)
        {
            FVector2D Screen;
            if (Enemy.Mesh && PC->ProjectWorldLocationToScreen(Enemy.Mesh->GetComponentLocation()+FVector(0,0,26),Screen)
                && Screen.X>20 && Screen.X<Left-20 && Screen.Y>110 && Screen.Y<H-120)
            {
                DrawRect(FLinearColor(.12f,.03f,.03f),Screen.X-14,Screen.Y,28,3);
                DrawRect(Accent,Screen.X-14,Screen.Y,28*FMath::Clamp(Enemy.Health/Enemy.MaxHealth,0.f,1.f),3);
            }
        }
        DrawRect(FLinearColor(.018f,.027f,.045f,.97f),0,H-118,Left,118);
        Label(Board->SelectedDescription(),24,H-108,Ink,.8f);
        const TCHAR* Actions[]={TEXT("PLACE / P"),TEXT("CLEAR GEMS / C"),TEXT("MERGE 2 / 2"),TEXT("MERGE 4 / 4"),TEXT("RECIPES / T"),TEXT("UPGRADE / V"),TEXT("REMOVE / X")};
        const bool BuildPhase=Board->Phase==ERoundPhase::Placing || Board->Phase==ERoundPhase::Choosing;
        const bool Enabled[]={Board->CanKeepSelected(),BuildPhase,Board->CanMerge(2),Board->CanMerge(4),true,BuildPhase && Board->Selected>=0,BuildPhase && Board->Pieces.IsValidIndex(Board->Selected) && Board->Pieces[Board->Selected].bRock};
        const float ButtonWidth=(Left-96)/7;
        for (int32 I=0;I<7;++I)
        {
            const float BX=24+I*(ButtonWidth+8);
            const bool HotAction=PC->GetMousePosition(MouseX,MouseY) && MouseX>=BX && MouseX<=BX+ButtonWidth && MouseY>=H-80 && MouseY<=H-44;
            DrawRect(!Enabled[I]?FLinearColor(.04f,.065f,.085f):HotAction?FLinearColor(.12f,.30f,.35f):FLinearColor(.06f,.20f,.23f),BX,H-80,ButtonWidth,36);
            Label(Actions[I],BX+9,H-68,Enabled[I]?Accent:Muted,.7f);
        }
        if (PC->bRecipesOpen)
        {
            DrawRect(FLinearColor(.025f,.04f,.06f,.98f),16,112,Left-32,H-246);
            Label(TEXT("CLASSIC RECIPES / Select an ingredient, then click a recipe to combine"),30,126,Accent,.83f);
            const float RowHeight=FMath::Min(42.f,(H-300.f)/FMath::Max(1,Board->RecipeCount()));
            for (int32 I=0;I<Board->RecipeCount();++I)
            {
                const float RowY=156+I*RowHeight;
                const bool Ready=Board->CanCraft(I);
                DrawRect(Ready?FLinearColor(.08f,.25f,.2f):FLinearColor(.045f,.07f,.10f),24,RowY,Left-48,RowHeight-3);
                FString Name,Ingredients;
                Board->RecipeDescription(I).Split(TEXT(" = "),&Name,&Ingredients);
                Label(Name,34,RowY+2,Ready?Accent:Ink,.78f);
                Label(Ingredients,34,RowY+18,Muted,FMath::Min(.72f,(Left-75)/FMath::Max(1.f,Ingredients.Len()*8.f)));
            }
        }
    }
    Label(PC->Status,24,H-35,Ink,.9f);
}
AGemGameMode::AGemGameMode()
{
    PlayerControllerClass=AGemController::StaticClass(); HUDClass=AGemHUD::StaticClass(); DefaultPawnClass=APawn::StaticClass();
}
