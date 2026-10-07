#include "GemPrototype.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DrawDebugHelpers.h"

FString AGemBoard::SelectedDescription() const
{
    if (Enemies.IsValidIndex(SelectedEnemy)) return FString::Printf(TEXT("%s enemy / %.0f of %.0f health"),Enemies[SelectedEnemy].bFlying?TEXT("Flying"):TEXT("Ground"),Enemies[SelectedEnemy].Health,Enemies[SelectedEnemy].MaxHealth);
    if (!Pieces.IsValidIndex(Selected)) return TEXT("Select a gem or stone");
    if (Pieces[Selected].bRock) return TEXT("Stone block / remove with X");
    const auto* Def=Definition(Selected);
    if (!Def) return TEXT("Gem definition missing");
    return FString::Printf(TEXT("%s / %.0f-%.0f dmg / %.0f range"),*Def->Name,Def->Damage+Def->Dice,Def->Damage+Def->Dice*Def->Sides,Def->Range);
}
int32 AGemBoard::PieceAt(FIntPoint P) const
{
    for (int32 I=Occupied.Num()-1;I>=0;--I) if (FMath::Abs(P.X-Occupied[I].X)<=1 && FMath::Abs(P.Y-Occupied[I].Y)<=1) return I;
    return INDEX_NONE;
}
void AGemBoard::SelectAt(FIntPoint P) { Selected=PieceAt(P); SelectedEnemy=INDEX_NONE; }
void AGemBoard::SelectEnemy(int32 Index) { SelectedEnemy=Enemies.IsValidIndex(Index)?Index:INDEX_NONE; Selected=INDEX_NONE; }
bool AGemBoard::SelectRay(FVector Origin,FVector Direction)
{
    float Closest=10000.f; int32 Gem=INDEX_NONE,Enemy=INDEX_NONE;
    auto Hit=[&](UStaticMeshComponent* Mesh)
    {
        if (!Mesh || !Mesh->GetStaticMesh()) return false;
        const FBox Box=Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetComponentTransform()).ExpandBy(8.f);
        float Near=0,Far=Closest;
        for (int32 Axis=0;Axis<3;++Axis)
        {
            if (FMath::Abs(Direction[Axis])<.000001f)
            { if (Origin[Axis]<Box.Min[Axis] || Origin[Axis]>Box.Max[Axis]) return false; }
            else
            {
                float A=(Box.Min[Axis]-Origin[Axis])/Direction[Axis],B=(Box.Max[Axis]-Origin[Axis])/Direction[Axis];
                if (A>B) Swap(A,B);
                Near=FMath::Max(Near,A); Far=FMath::Min(Far,B);
                if (Near>Far) return false;
            }
        }
        if (Near>=Closest) return false;
        Closest=Near; return true;
    };
    for (int32 I=0;I<Placed.Num();++I) if (Hit(Placed[I])) { Gem=I; Enemy=INDEX_NONE; }
    for (int32 I=0;I<Enemies.Num();++I) if (Enemies[I].Health>0 && Hit(Enemies[I].Mesh)) { Enemy=I; Gem=INDEX_NONE; }
    Selected=Gem; SelectedEnemy=Enemy;
    return Gem!=INDEX_NONE || Enemy!=INDEX_NONE;
}

void AGemBoard::ResetRun()
{
    ClearGems(); for (auto& Enemy:Enemies) { if (Enemy.Outline) Enemy.Outline->DestroyComponent(); if (Enemy.Mesh) Enemy.Mesh->DestroyComponent(); }
    Enemies.Reset(); Areas.Reset(); TowerMana.Reset();
    Wave=1; Lives=20; Score=0; OffersPlaced=0; Selected=INDEX_NONE; Phase=ERoundPhase::Placing;
    RebuildRoute();
}
void AGemBoard::ApplyPieceVisual(int32 I)
{
    if (!Pieces.IsValidIndex(I)) return;
    const auto* Def=FindDefinition(Pieces[I].DefinitionId.ToString());
    if(!Def) return;
    UStaticMesh* Model=LoadObject<UStaticMesh>(nullptr,*Def->Model); if(!Model) return;
    Placed[I]->SetStaticMesh(Model); Placed[I]->EmptyOverrideMaterials();
    Placed[I]->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*Def->Material));
    ApplyGemSize(Placed[I]);
    FVector Position=PlacementPosition(Occupied[I]);
    if(Pieces[I].bRock)
        Position.Z=8.f-Model->GetBoundingBox().Min.Z*Placed[I]->GetComponentScale().Z-.75f;
    Placed[I]->SetWorldLocation(Position);
    if (Outlines.IsValidIndex(I)) Outlines[I]->SetStaticMesh(Placed[I]->GetStaticMesh());
}
UStaticMeshComponent* AGemBoard::CreateOutline(UStaticMeshComponent* Mesh)
{
    auto* Outline=NewObject<UStaticMeshComponent>(this);
    Outline->SetupAttachment(Mesh); Outline->SetStaticMesh(Mesh->GetStaticMesh());
    Outline->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_CelOutline.M_CelOutline")));
    Outline->SetCollisionEnabled(ECollisionEnabled::NoCollision); Outline->SetCastShadow(false);
    Outline->RegisterComponent(); return Outline;
}
void AGemBoard::MakeRock(int32 I) { Pieces[I].bRock=true; Pieces[I].bPending=false; Pieces[I].DefinitionId=FName(*RockDefinition); ApplyPieceVisual(I); }
bool AGemBoard::KeepSelected()
{
    if (!CanKeepSelected()) return false;
    for (int32 I=0;I<Pieces.Num();++I) if (Pieces[I].bPending)
    { if (I==Selected) Pieces[I].bPending=false; else MakeRock(I); }
    StartWave(); return true;
}
bool AGemBoard::CanKeepSelected() const
{
    return Phase==ERoundPhase::Choosing && OffersPlaced==5 && SelectedEnemy==INDEX_NONE && Pieces.IsValidIndex(Selected) && !Pieces[Selected].bRock && Pieces[Selected].bPending;
}
bool AGemBoard::ClearAllGems()
{
    if (Phase!=ERoundPhase::Placing && Phase!=ERoundPhase::Choosing) return false;
    ClearGems(); OffersPlaced=0; Phase=ERoundPhase::Placing; RebuildRoute(); return true;
}
bool AGemBoard::CanMerge(int32 Count) const
{
    if (Phase!=ERoundPhase::Choosing || !MergeRules.Contains(Count) || !Pieces.IsValidIndex(Selected)) return false;
    const auto& P=Pieces[Selected]; if (P.bRock || !MergeResult(Count)) return false;
    int32 Found=0;
    for (const auto& Other:Pieces) if (!Other.bRock && Other.DefinitionId==P.DefinitionId) ++Found;
    return Found>=Count;
}
bool AGemBoard::MergeSelected(int32 Count)
{
    if (!CanMerge(Count)) return false;
    const auto Original=Pieces[Selected]; int32 Needed=Count-1;
    for (int32 I=0;I<Pieces.Num() && Needed>0;++I)
        if (I!=Selected && !Pieces[I].bRock && Pieces[I].DefinitionId==Original.DefinitionId) { MakeRock(I); --Needed; }
    Pieces[Selected].DefinitionId=FName(*MergeResult(Count)->Id); Pieces[Selected].bPending=true; ApplyPieceVisual(Selected);
    return KeepSelected();
}
TArray<int32> AGemBoard::FindRecipePieces(int32 Index) const
{
    TArray<int32> Matches;
    if (!Recipes.IsValidIndex(Index) || !Pieces.IsValidIndex(Selected) || Pieces[Selected].bRock) return Matches;
    for (const FString& Required:Recipes[Index].Ingredients)
    {
        int32 Found=INDEX_NONE;
        // Prefer the selected gem whenever it matches a recipe ingredient.
        for (int32 Pass=0;Pass<2 && Found<0;++Pass) for (int32 I=0;I<Pieces.Num();++I)
        {
            if ((Pass==0 && I!=Selected) || Matches.Contains(I)) continue;
            const auto* Def=Definition(I); if (Def && Def->Id==Required) { Found=I; break; }
        }
        if (Found<0) return {};
        Matches.Add(Found);
    }
    if (!Matches.Contains(Selected)) return {};
    return Matches;
}
bool AGemBoard::CanCraft(int32 Recipe) const
{ return Phase==ERoundPhase::Choosing && Recipes.IsValidIndex(Recipe) && FindRecipePieces(Recipe).Num()==Recipes[Recipe].Ingredients.Num(); }
bool AGemBoard::CraftSelected(int32 Recipe)
{
    if (!CanCraft(Recipe)) return false;
    const auto* Def=Definitions.Find(Recipes[Recipe].Result); if (!Def) return false;
    const auto Matches=FindRecipePieces(Recipe);
    for (int32 I:Matches) if (I!=Selected) MakeRock(I);
    Pieces[Selected].DefinitionId=FName(*Def->Id); Pieces[Selected].Level=Def->InitialLevel;
    Pieces[Selected].bPending=true; ApplyPieceVisual(Selected);
    return KeepSelected();
}
bool AGemBoard::UpgradeSelectedTower()
{
    if (Phase!=ERoundPhase::Placing && Phase!=ERoundPhase::Choosing) return false;
    const auto* Def=Definition(Selected);
    auto* PC=Cast<AGemController>(GetWorld()->GetFirstPlayerController());
    if (!Def || !PC || Def->Upgrade.IsEmpty() || Def->UpgradeCost<=0 || PC->Gold<Def->UpgradeCost || !Definitions.Contains(Def->Upgrade)) return false;
    PC->Gold-=Def->UpgradeCost; Pieces[Selected].DefinitionId=FName(*Def->Upgrade); ApplyPieceVisual(Selected); return true;
}
FString AGemBoard::RecipeDescription(int32 I) const
{
    if (!Recipes.IsValidIndex(I)) return TEXT("");
    FString Result=Recipes[I].Name+TEXT(" = ");
    for (int32 N=0;N<Recipes[I].Ingredients.Num();++N)
    { if (N) Result+=TEXT(" + "); const auto* Def=Definitions.Find(Recipes[I].Ingredients[N]); Result+=Def?Def->Name:Recipes[I].Ingredients[N]; }
    return Result;
}
bool AGemBoard::DemolishSelectedRock()
{
    if ((Phase!=ERoundPhase::Placing && Phase!=ERoundPhase::Choosing) || !Pieces.IsValidIndex(Selected) || !Pieces[Selected].bRock) return false;
    Remove(Occupied[Selected]); return true;
}
void AGemBoard::StartWave()
{
    RebuildRoute(); if (Route.IsEmpty()) return;
    Phase=ERoundPhase::Combat; Spawned=0; SpawnTimer=0; Shots.Reset();
    WaveSize=Waves.IsValidIndex(Wave-1)?Waves[Wave-1].Count:10;
    TowerMana.Init(0,Pieces.Num());
}
void AGemBoard::EndCombat()
{
    Shots.Reset(); Areas.Reset();
    for(auto& P:Pieces) P.TargetIds.Reset();
    if (Lives<=0) { Phase=ERoundPhase::Defeat; return; }
    if (Wave>=Waves.Num()) { Phase=ERoundPhase::Victory; return; }
    ++Wave; OffersPlaced=0; Selected=INDEX_NONE; Phase=ERoundPhase::Placing;
}
void AGemBoard::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (Selected>=0 && Placed.IsValidIndex(Selected))
        DrawDebugBox(GetWorld(),PlacementPosition(Occupied[Selected])+FVector(0,0,25),FVector(49,49,25),FColor::Yellow,false,-1,0,2);
    if (Enemies.IsValidIndex(SelectedEnemy)) DrawDebugSphere(GetWorld(),Enemies[SelectedEnemy].Mesh->GetComponentLocation(),24,12,FColor::Yellow,false,-1,0,2);
    if (Phase!=ERoundPhase::Combat || !DefinitionErrors.IsEmpty() || !Waves.IsValidIndex(Wave-1)) return;
    const float DT=FMath::Min(DeltaTime,.1f);
    const auto& Current=Waves[Wave-1]; SpawnTimer-=DT;
    if (Spawned<WaveSize && SpawnTimer<=0)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(this); Mesh->SetupAttachment(RootComponent);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_Cel_Enemy.M_Cel_Enemy")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent(); Mesh->SetWorldScale3D(FVector(.28f));
        FGemEnemy E; E.Mesh=Mesh; E.Outline=CreateOutline(Mesh); E.Health=E.MaxHealth=Current.Health; E.Id=NextEnemyId++; E.bFlying=Current.bFlying;
        Mesh->SetWorldLocation(E.bFlying?AirRoute[0]:Route[0]); Enemies.Add(E);
        ++Spawned; SpawnTimer=Current.SpawnInterval;
    }
    for (auto& E:Enemies)
    {
        if(E.Health<=0) continue;
        UpdateEnemyAuras(E);
        E.SlowTime=FMath::Max(0.f,E.SlowTime-DT); E.StunTime=FMath::Max(0.f,E.StunTime-DT); E.ArmorTime=FMath::Max(0.f,E.ArmorTime-DT);
        if (E.PoisonTime>0) { DamageEnemy(E,E.Poison*FMath::Min(DT,E.PoisonTime),E.PoisonTower,false); E.PoisonTime=FMath::Max(0.f,E.PoisonTime-DT); }
        for (const auto& Area:Areas) if (FVector::Dist2D(E.Mesh->GetComponentLocation(),Area.Center)<=Area.Radius) DamageEnemy(E,Area.Damage*FMath::Min(DT,Area.Remaining),Area.Tower);
        if(E.Health<=0) continue;
        const float Speed=Current.Speed*(E.SlowTime>0?1-E.Slow:1)*(E.PoisonTime>0?1-E.PoisonSlow:1)*(1-E.AuraSlow);
        E.EffectiveSpeed=E.StunTime>0?0:Speed;
        const auto& Path=E.bFlying?AirRoute:Route;
        float Travel=E.StunTime>0?0:Speed*DT;
        while (Travel>0 && E.RouteIndex<Path.Num())
        {
            const FVector Position=E.Mesh->GetComponentLocation(),Delta=Path[E.RouteIndex]-Position;
            const float Distance=Delta.Size();
            if (Distance<=Travel) { E.Mesh->SetWorldLocation(Path[E.RouteIndex++]); Travel-=Distance; }
            else { E.Mesh->SetWorldLocation(Position+Delta.GetSafeNormal()*Travel); Travel=0; }
        }
        if (E.RouteIndex>=Path.Num() && E.Health>0) { --Lives; E.Health=0; E.bLeaked=true; }
    }
    for(auto& Area:Areas) Area.Remaining-=DT;
    Areas.RemoveAll([](const auto& A){return A.Remaining<=0;});
    for(int32 I=Shots.Num()-1;I>=0;--I)
    {
        Shots[I].Remaining-=DT;
        if(Shots[I].Remaining<=0) { const auto Shot=Shots[I]; Shots.RemoveAt(I); ApplyHit(Shot); }
    }
    for (int32 I=0;I<Pieces.Num();++I)
    {
        if (Pieces[I].bPending) continue;
        const auto* D=Definition(I); if (!D) continue;
        float Haste=0,DamageAura=0;
        TMap<FString,float> HasteChannels,DamageChannels;
        for (int32 J=0;J<Pieces.Num();++J)
        {
            if (Pieces[J].bPending) continue;
            const auto* Aura=Definition(J); if (!Aura) continue;
            for (const auto& M:Aura->Modifiers)
                if (FVector::Dist2D(Placed[I]->GetComponentLocation(),Placed[J]->GetComponentLocation())<=M.Get(TEXT("radius")))
                {
                    auto* Channels=M.Type==TEXT("attack_speed_aura")?&HasteChannels:M.Type==TEXT("damage_aura")?&DamageChannels:nullptr;
                    if (Channels) { float& V=Channels->FindOrAdd(M.Channel); V=FMath::Max(V,M.Get(TEXT("fraction"))); }
                }
        }
        for (const auto& V:HasteChannels) Haste+=V.Value;
        for (const auto& V:DamageChannels) DamageAura+=V.Value;
        Pieces[I].SpeedAura=Haste; Pieces[I].DamageAura=DamageAura; Pieces[I].TargetIds.Reset();
        AttackCooldown[I]-=DT;
        const FGemModifier* Mana=nullptr; for (const auto& M:D->Modifiers) if (M.Type==TEXT("mana")) Mana=&M;
        if (Mana) TowerMana[I]=FMath::Min(Mana->Get(TEXT("maximum")),TowerMana[I]+Mana->Get(TEXT("regeneration"))*DT);
        TArray<int32> Targets;
        for (int32 N=0;N<Enemies.Num();++N)
            if (Enemies[N].Health>0 && (Enemies[N].bFlying?D->bAir:D->bGround) && FVector::Dist2D(Enemies[N].Mesh->GetComponentLocation(),Placed[I]->GetComponentLocation())<=D->Range) Targets.Add(N);
        for(int32 T=0;T<FMath::Min(D->Targets,Targets.Num());++T) Pieces[I].TargetIds.Add(Enemies[Targets[T]].Id);
        if (Targets.IsEmpty() || AttackCooldown[I]>0) continue;
        AttackCooldown[I]=D->Interval*FMath::Max(.05f,1-Haste);
        for (int32 T=0;T<FMath::Min(D->Targets,Targets.Num());++T)
        {
            auto& E=Enemies[Targets[T]]; float Damage=D->Damage;
            for (int32 Die=0;Die<D->Dice;++Die) Damage+=FMath::RandRange(1,D->Sides);
            Damage*=(1+DamageAura)*(1+Pieces[I].Level*D->DamagePerLevel);
            for(const auto& M:D->Modifiers) if(M.Type==TEXT("critical") && FMath::FRand()<M.Get(TEXT("chance"))) Damage*=M.Get(TEXT("multiplier"));
            FGemShot Shot; Shot.Tower=I; Shot.EnemyId=E.Id; Shot.Damage=Damage;
            Shot.Origin=Placed[I]->GetComponentLocation()+FVector(0,0,45);
            Shot.Remaining=D->bInstant?0:FVector::Dist(Shot.Origin,E.Mesh->GetComponentLocation())/D->ProjectileSpeed;
            if(D->bInstant) ApplyHit(Shot); else Shots.Add(Shot);
            DrawDebugLine(GetWorld(),Shot.Origin,E.Mesh->GetComponentLocation(),FColor::Cyan,false,FMath::Max(.12f,Shot.Remaining),0,2);
        }
    }
    for (int32 I=Enemies.Num()-1;I>=0;--I) if (Enemies[I].Health<=0)
    {
        if (!Enemies[I].bLeaked) { Score+=10*Wave; if (auto* PC=Cast<AGemController>(GetWorld()->GetFirstPlayerController())) PC->Gold+=Current.Reward; }
        if (Enemies[I].Outline) Enemies[I].Outline->DestroyComponent();
        Enemies[I].Mesh->DestroyComponent(); Enemies.RemoveAt(I);
        if (SelectedEnemy==I) SelectedEnemy=INDEX_NONE;
        else if (SelectedEnemy>I) --SelectedEnemy;
    }
    if (Lives<=0) { Phase=ERoundPhase::Defeat; return; }
    if (Spawned>=WaveSize && Enemies.IsEmpty()) EndCombat();
}
