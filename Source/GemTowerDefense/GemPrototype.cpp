#include "GemPrototype.h"
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
        TEXT("ERICISSSICIRRRICISSS"),TEXT("SSSISSSSSISSSSSISSSS"),
        TEXT("SSSRSSSSSRSSSSSRSSSS"),TEXT("SSSRSSSSSRSSSSSRSSSS"),
        TEXT("SSSRSSSSSRSSSSSRSSSS"),TEXT("SSSRSSSSSRSSSSSRSSSS"),
        TEXT("SSSRSSSSSRSSSSSRSSSS"),TEXT("SSSISSSSSISSSSSISSSS"),
        TEXT("SSSCIRRRRRRRRRRCISSS"),TEXT("SSSISSSSSRSSSSSISSSS"),
        TEXT("SSSSSSSSSRSSSSSSSSSS"),TEXT("SSSSSSSSSRSSSSSSSSSS"),
        TEXT("SSSSSSSSSRSSSSSSSSSS"),TEXT("SSSSSSSSSISSSSSSSSSS"),
        TEXT("SSSSSSSSICIRRRRRRRRX"),TEXT("SSSSSSSSSSSSSSSSSSSS"),
        TEXT("SSSSSSSSSSSSSSSSSSSS"),TEXT("SSSSSSSSSSSSSSSSSSSS")};
    TArray<FString> Rows;
    const bool Loaded=LayoutRows.Num()>0 ? (Rows=LayoutRows,true) : FFileHelper::LoadFileToStringArray(Rows,*(FPaths::ProjectDir()/TEXT("Maps/BoardLayout.txt")));
    bool Valid=Loaded && Rows.Num()==GridSize;
    int32 Entries=0,Exits=0;
    for (const FString& Row:Rows)
    {
        if (Row.Len()!=GridSize) Valid=false;
        for (TCHAR Symbol:Row)
        {
            if (!FString(TEXT("SGRICEX")).Contains(FString::Chr(Symbol))) Valid=false;
            Entries+=Symbol=='E'; Exits+=Symbol=='X';
        }
    }
    if (Entries!=1 || Exits!=1) Valid=false;
    if (!Valid)
    {
        if (Loaded) UE_LOG(LogTemp,Warning,TEXT("Invalid BoardLayout.txt; using embedded 20x20 layout."));
        Rows.Reset(); for (const TCHAR* Row:DefaultRows) Rows.Add(Row);
    }
    LayoutRows=Rows;
    if (CheckpointOrder.IsEmpty())
    {
        const FIntPoint ClassicOrder[]={FIntPoint(3,2),FIntPoint(3,10),FIntPoint(9,2),FIntPoint(15,2),FIntPoint(15,10),FIntPoint(9,16)};
        bool Classic=true;
        for (FIntPoint P:ClassicOrder) if (Rows[P.Y][P.X]!='C') Classic=false;
        if (Classic) for (FIntPoint P:ClassicOrder) CheckpointOrder.Add(P);
        else for (int32 Y=0;Y<20;++Y) for (int32 X=0;X<20;++X) if (Rows[Y][X]=='C') CheckpointOrder.Add(FIntPoint(X,Y));
    }
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
    if (OffersPlaced==5) Phase=ERoundPhase::Choosing;
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
    Pieces.Reset(); AttackCooldown.Reset(); Selected=INDEX_NONE;
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
    for (int32 Attempt=0;Attempt<50 && Centers.Num()<5;++Attempt)
    {
        Centers.Reset();
        for (int32 Trial=0;Trial<1000 && Centers.Num()<5;++Trial)
        {
            FIntPoint P(Random.RandRange(2,17),Random.RandRange(2,17));
            bool Fits=true;
            for (FIntPoint C:Centers) if (FMath::Abs(P.X-C.X)+FMath::Abs(P.Y-C.Y)<6) Fits=false;
            if (Fits) Centers.Add(P);
        }
    }
    if (Centers.Num()!=5) Centers={FIntPoint(3,3),FIntPoint(15,3),FIntPoint(9,9),FIntPoint(3,16),FIntPoint(16,16)};
    const FIntPoint Directions[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)};
    TArray<int32> CheckpointOwner; CheckpointOwner.Init(-1,400);
    TArray<bool> Closed; Closed.Init(false,400);
    TArray<TArray<int32>> Ports; Ports.SetNum(5);
    auto Id=[](FIntPoint P) { return P.Y*GridSize+P.X; };
    for (int32 I=0;I<5;++I)
    {
        const FIntPoint C=Centers[I]; LayoutRows[C.Y][C.X]='C'; CheckpointOwner[Id(C)]=I;
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
    for (int32 I=1;I<5;++I)
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
                if (Previous[N]!=-2 || Closed[N] || LayoutRows[Next.Y][Next.X]=='C') continue;
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
                if (Previous[N]!=-2 || Closed[N] || LayoutRows[Next.Y][Next.X]=='C') continue;
                Previous[N]=Here; Queue.Add(N);
            }
        }
        checkf(Found>=0,TEXT("Boundary endpoint must connect to its checkpoint"));
        for (int32 P=Found;P>=0;P=Previous[P])
            if (LayoutRows[P/20][P%20]=='S') LayoutRows[P/20][P%20]='R';
        LayoutRows[Endpoint.Y][Endpoint.X]=Symbol;
    };
    ConnectEdge(Entry,0,'E'); ConnectEdge(Exit,4,'X');
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
AGemController::AGemController() { bShowMouseCursor=true; bAutoManageActiveCameraTarget=false; DefaultMouseCursor=EMouseCursor::Crosshairs; }
void AGemController::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AGemBoard> It(GetWorld());It;++It) { Board=*It; break; }
    if (!Board) Board=GetWorld()->SpawnActor<AGemBoard>();
    SetViewTarget(Board);
    FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode);
    if (FParse::Param(FCommandLine::Get(),TEXT("GemSmokeTest")))
    {
        for (int32 I=0;I<8;++I) Board->Place(FIntPoint(8+(I%4)*4,10+(I/4)*6),I);
        FTimerHandle CaptureTimer,ExitTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer,FTimerDelegate::CreateLambda([this]()
        {
            int32 Width,Height; GetViewportSize(Width,Height);
            UE_LOG(LogTemp,Display,TEXT("GEM_CAMERA viewport=%dx%d width=%.1f location=%s target=%s"),Width,Height,Board->Camera->OrthoWidth,*Board->Camera->GetComponentLocation().ToString(),*GetNameSafe(GetViewTarget()));
            UE_LOG(LogTemp,Display,TEXT("GEM_POV ortho=%d width=%.1f location=%s"),PlayerCameraManager->IsOrthographic(),PlayerCameraManager->GetOrthoWidth(),*PlayerCameraManager->GetCameraLocation().ToString());
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("PrototypePreview.png"),true,false);
        }),8.f,false);
        GetWorldTimerManager().SetTimer(ExitTimer,FTimerDelegate::CreateLambda([this]()
        { ConsoleCommand(TEXT("quit")); }),12.f,false);
    }
}
void AGemController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!Board) return;
    if (GetViewTarget()!=Board) SetViewTarget(Board);
    int32 W,H; GetViewportSize(W,H); if (W<=0 || H<=0) return;
    const float Side=SidebarWidth(W),Header=64.f;
    const float Aspect=float(W)/H;
    const float Ortho=FMath::Max(2828.f/(1-Side/W),1900.f*Aspect/(1-Header/H))*1.10f;
    Board->Camera->SetOrthoWidth(Ortho);
    PlayerCameraManager->bIsOrthographic=true;
    PlayerCameraManager->bAutoCalculateOrthoPlanes=false;
    PlayerCameraManager->bUpdateOrthoPlanes=false;
    PlayerCameraManager->SetOrthoWidth(Ortho);
    const FVector Right=Board->Camera->GetRightVector(), Up=Board->Camera->GetUpVector();
    Board->Camera->SetWorldLocation(-Board->Camera->GetForwardVector()*3500.f + Right*(Ortho*Side/W*.5f) + Up*(Ortho/Aspect*Header/H*.5f));
    if (WasInputKeyJustPressed(EKeys::R)) { Board->Regenerate(); Status=TEXT("Generated 5 connected T-shaped checkpoints"); }
    if (WasInputKeyJustPressed(EKeys::D)) { Board->LoadDefaultLayout(); Status=TEXT("Default layout restored"); }
    float X,Y; bHover=false;
    if (GetMousePosition(X,Y) && WasInputKeyJustPressed(EKeys::LeftMouseButton) && X>W-Side+24 && X<W-24 && Y>=458 && Y<=500)
    { Board->Regenerate(); Status=TEXT("Generated 5 connected T-shaped checkpoints"); }
    if (GetMousePosition(X,Y) && WasInputKeyJustPressed(EKeys::LeftMouseButton) && X>W-Side+24 && X<W-24 && Y>=700 && Y<=742)
    { UpgradeChance(); }
    if (WasInputKeyJustPressed(EKeys::U)) UpgradeChance();
    if (GetMousePosition(X,Y) && X<W-Side && Y>Header)
    {
        FVector Origin,Direction;
        if (DeprojectScreenPositionToWorld(X,Y,Origin,Direction) && FMath::Abs(Direction.Z)>.001f)
        {
            const float T=(10.f-Origin.Z)/Direction.Z;
            Hover=Board->Snap(Origin+Direction*T);
            bHover=T>0 && Hover.X>=0 && Hover.Y>=0 && Hover.X<=38 && Hover.Y<=38;
            if (bHover && WasInputKeyJustPressed(EKeys::LeftMouseButton))
            {
                if (Board->CanPlace(Hover))
                {
                    const int32 Type=FMath::RandRange(0,7),Quality=RollQuality();
                    Board->Place(Hover,Type,Quality);
                    Status=FString::Printf(TEXT("Placed %s %s"),QualityNames[Quality],GemNames[Type]);
                }
                else Status=TEXT("Blocked: overlap, protected tile, or no route from entry to exit");
            }
            if (bHover && WasInputKeyJustPressed(EKeys::RightMouseButton)) Board->Remove(Hover);
        }
    }
    Board->UpdateGhost(Hover,0,bHover);
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
    Label(TEXT("SCORE  0   |   WAVE  --   |   LIVES  20"),W*.43f,23,Ink,.85f);
    Label(FString::Printf(TEXT("CHANCE LEVEL  %d / 8"),PC->ChanceLevel),W*.76f,23,Accent,.85f);
    Label(TEXT("MAP LAYOUT / 20 x 20"),Left+24,90,Accent,.95f);
    Label(TEXT("Live tile array"),Left+24,120,Muted,.85f);
    if (PC->Board)
    {
        const float Step=(Side-48)/20.f;
        for (int32 Y=0;Y<PC->Board->LayoutRows.Num();++Y)
            for (int32 X=0;X<PC->Board->LayoutRows[Y].Len();++X)
            {
                const TCHAR C=PC->Board->LayoutRows[Y][X];
                FLinearColor Color=C=='E'?Accent:C=='X'?FLinearColor(1,.4f,.3f):C=='C'?FLinearColor(1,.72f,.2f):C=='I'?Accent:C=='R'?Ink:Muted;
                Label(FString::Chr(C),Left+24+X*Step,150+Y*13.f,Color,.72f);
            }
    }
    Label(TEXT("S  Snow / grass     R  Road"),Left+24,422,Ink,.75f);
    Label(TEXT("I  Intersection     C  Checkpoint"),Left+24,439,Ink,.75f);
    Label(TEXT("E  Entry     X  Exit  /  road tiles"),Left+24,405,Ink,.75f);
    float MouseX,MouseY; const bool Hot=PC->GetMousePosition(MouseX,MouseY) && MouseX>Left+24 && MouseX<W-24 && MouseY>=458 && MouseY<=500;
    DrawRect(Hot?FLinearColor(.12f,.38f,.35f):FLinearColor(.07f,.25f,.24f),Left+24,458,Side-48,42);
    Label(TEXT("REGENERATE  /  5 CHECKPOINTS"),Left+36,471,Accent,.8f);
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
    Label(TEXT("Left click place / Right click remove"),Left+24,782,Ink,.75f);
    if (PC->Board) Label(FString::Printf(TEXT("Gems placed %d / half-cell snapping"),PC->Board->GemCount()),Left+24,810,Muted,.75f);
    Label(PC->Status,24,H-35,Ink,.9f);
}
AGemGameMode::AGemGameMode()
{
    PlayerControllerClass=AGemController::StaticClass(); HUDClass=AGemHUD::StaticClass(); DefaultPawnClass=APawn::StaticClass();
}
