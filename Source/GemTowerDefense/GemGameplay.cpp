#include "GemPrototype.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DrawDebugHelpers.h"

static const TCHAR* Types[]={TEXT("Amethyst"),TEXT("Aquamarine"),TEXT("Diamond"),TEXT("Emerald"),TEXT("Opal"),TEXT("Ruby"),TEXT("Sapphire"),TEXT("Topaz")};
static const TCHAR* Grades[]={TEXT("Chipped"),TEXT("Flawed"),TEXT("Normal"),TEXT("Flawless"),TEXT("Perfect"),TEXT("Great")};
static TSharedPtr<FJsonObject> ReadData(const TCHAR* Name)
{
    FString Text; TSharedPtr<FJsonObject> Root;
    if (FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Data")/Name)))
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root);
    return Root;
}
static double Number(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,double Default=0)
{ double Value=Default; Object->TryGetNumberField(Key,Value); return Value; }
static FString String(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key)
{ FString Value; Object->TryGetStringField(Key,Value); return Value; }
static bool Boolean(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,bool Default=true)
{ bool Value=Default; Object->TryGetBoolField(Key,Value); return Value; }

void AGemBoard::LoadDefinitions()
{
    Definitions.Reset(); Recipes.Reset(); Waves.Reset();
    const auto Root=ReadData(TEXT("Gems.json"));
    if (Root)
    {
        for (const TCHAR* Group:{TEXT("base_gems"),TEXT("special_towers")})
        {
            const TArray<TSharedPtr<FJsonValue>>* Entries;
            if (!Root->TryGetArrayField(Group,Entries)) continue;
            for (const auto& Entry:*Entries)
            {
                const auto Object=Entry->AsObject(); if (!Object) continue;
                FTowerDefinition Def;
                Def.Id=String(Object,TEXT("id")); Def.Name=String(Object,TEXT("name")); Def.Model=String(Object,TEXT("model"));
                Def.Quality=Number(Object,TEXT("quality"));
                const FString Type=String(Object,TEXT("gem_type"));
                for (int32 I=0;I<8;++I) if (Type==Types[I]) Def.BaseType=I;
                const auto Stats=Object->GetObjectField(TEXT("stats"));
                Def.Damage=Number(Stats,TEXT("damage_base"),1); Def.Dice=Number(Stats,TEXT("damage_dice"),1);
                Def.Sides=Number(Stats,TEXT("damage_sides"),1); Def.Range=Number(Stats,TEXT("range"),300);
                Def.Interval=FMath::Max(.05f,float(Number(Stats,TEXT("attack_interval"),1)));
                Def.Targets=Number(Stats,TEXT("targets"),1); Def.bGround=Boolean(Stats,TEXT("attacks_ground")); Def.bAir=Boolean(Stats,TEXT("attacks_air"));
                Def.Upgrade=String(Object,TEXT("upgrades_to")); Def.UpgradeCost=Number(Object,TEXT("upgrade_cost"));
                const TArray<TSharedPtr<FJsonValue>>* Modifiers;
                if (Object->TryGetArrayField(TEXT("modifiers"),Modifiers)) for (const auto& Modifier:*Modifiers)
                {
                    FGemModifier Mod; const auto M=Modifier->AsObject(); Mod.Type=String(M,TEXT("type")); Mod.Channel=String(M,TEXT("channel"));
                    for (const auto& Pair:M->Values)
                    {
                        double Value; bool Flag;
                        if (Pair.Value->TryGetNumber(Value)) Mod.Values.Add(FString(*Pair.Key),float(Value));
                        else if (Pair.Value->TryGetBool(Flag)) Mod.Values.Add(FString(*Pair.Key),Flag?1.f:0.f);
                    }
                    Def.Modifiers.Add(Mod);
                }
                if (!Def.Id.IsEmpty()) Definitions.Add(Def.Id,Def);
            }
        }
    }
    const auto RecipeRoot=ReadData(TEXT("Recipes.json"));
    if (RecipeRoot) for (const auto& Entry:RecipeRoot->GetArrayField(TEXT("recipes")))
    {
        const auto Object=Entry->AsObject(); FGemRecipe Recipe;
        Recipe.Name=String(Object,TEXT("name")); Recipe.Result=String(Object,TEXT("result"));
        for (const auto& Ingredient:Object->GetArrayField(TEXT("ingredients")))
        {
            const auto Item=Ingredient->AsObject();
            for (int32 I=0;I<int32(Number(Item,TEXT("count"),1));++I) Recipe.Ingredients.Add(String(Item,TEXT("id")));
        }
        Recipes.Add(Recipe);
    }
    const auto WaveRoot=ReadData(TEXT("Waves.json"));
    if (WaveRoot) for (const auto& Entry:WaveRoot->GetArrayField(TEXT("waves")))
    {
        const auto Object=Entry->AsObject(); FWaveDefinition Def;
        Def.Count=Number(Object,TEXT("count"),10); Def.Health=Number(Object,TEXT("health"),40);
        Def.Speed=Number(Object,TEXT("speed"),100); Def.SpawnInterval=Number(Object,TEXT("spawn_interval"),.65);
        Def.Reward=Number(Object,TEXT("reward"),5); Def.Armor=Number(Object,TEXT("armor"));
        Def.bFlying=Boolean(Object,TEXT("flying"),false); Waves.Add(Def);
    }
    if (Waves.IsEmpty()) for (int32 I=0;I<20;++I) { FWaveDefinition Def; Def.Health=40*FMath::Pow(1.2f,I); Def.bFlying=(I+1)%4==0; Waves.Add(Def); }
}
const FTowerDefinition* AGemBoard::Definition(int32 Index) const
{
    if (!Pieces.IsValidIndex(Index) || Pieces[Index].bRock) return nullptr;
    const auto& Piece=Pieces[Index];
    return Definitions.Find(Piece.Special.IsNone()?FString::Printf(TEXT("%s_%s"),Types[Piece.Type],Grades[Piece.Quality]):Piece.Special.ToString());
}
FString AGemBoard::SelectedDescription() const
{
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
void AGemBoard::SelectAt(FIntPoint P) { Selected=PieceAt(P); }
bool AGemBoard::FindRoute(TArray<FVector>& Result,const FIntPoint* ExtraBlock) const
{
    Result.Reset();
    const FIntPoint Directions[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)};
    TArray<FIntPoint> Goals;
    Goals.Add(EntryCell*2);
    for (FIntPoint C:CheckpointOrder) Goals.Add(C*2);
    Goals.Add(ExitCell*2);
    auto Blocked=[&](FIntPoint P)
    {
        if (ExtraBlock && FMath::Abs(P.X-ExtraBlock->X)<2 && FMath::Abs(P.Y-ExtraBlock->Y)<2) return true;
        for (FIntPoint O:Occupied) if (FMath::Abs(P.X-O.X)<2 && FMath::Abs(P.Y-O.Y)<2) return true;
        return false;
    };
    for (int32 Segment=1;Segment<Goals.Num();++Segment)
    {
        const FIntPoint Start=Goals[Segment-1],End=Goals[Segment];
        if (Blocked(Start) || Blocked(End)) return false;
        TArray<int32> Previous; Previous.Init(-2,39*39);
        TArray<FIntPoint> Queue; Queue.Add(Start); Previous[Start.Y*39+Start.X]=-1;
        bool Found=false;
        for (int32 Head=0;Head<Queue.Num();++Head)
        {
            const FIntPoint P=Queue[Head]; if (P==End) { Found=true; break; }
            for (FIntPoint D:Directions)
            {
                const FIntPoint N=P+D;
                if (N.X<0 || N.Y<0 || N.X>=39 || N.Y>=39 || Previous[N.Y*39+N.X]!=-2 || Blocked(N)) continue;
                Previous[N.Y*39+N.X]=P.Y*39+P.X; Queue.Add(N);
            }
        }
        if (!Found) return false;
        TArray<FVector> Part;
        for (int32 N=End.Y*39+End.X;N>=0;N=Previous[N]) Part.Add(PlacementPosition(FIntPoint(N%39,N/39))+FVector(0,0,18));
        for (int32 I=Part.Num()-1;I>=0;--I) if (Result.IsEmpty() || !Result.Last().Equals(Part[I])) Result.Add(Part[I]);
    }
    return true;
}
bool AGemBoard::HasValidRoute() const { TArray<FVector> Test; return FindRoute(Test); }
void AGemBoard::RebuildRoute()
{
    FindRoute(Route); AirRoute.Reset();
    if (!Route.IsEmpty())
    {
        AirRoute.Add(Route[0]+FVector(0,0,60));
        for (FIntPoint C:CheckpointOrder) AirRoute.Add(PlacementPosition(C*2)+FVector(0,0,78));
        AirRoute.Add(Route.Last()+FVector(0,0,60));
    }
    RouteMarkers->ClearInstances();
    RouteMarkers->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_Path.M_Path")));
    for (int32 I=0;I<Route.Num();I+=2) RouteMarkers->AddInstance(FTransform(FRotator::ZeroRotator,Route[I]-FVector(0,0,16),FVector(.07f)));
}
void AGemBoard::ResetRun()
{
    ClearGems(); for (auto& Enemy:Enemies) if (Enemy.Mesh) Enemy.Mesh->DestroyComponent();
    Enemies.Reset(); Areas.Reset(); TowerMana.Reset();
    Wave=1; Lives=20; Score=0; OffersPlaced=0; Selected=INDEX_NONE; Phase=ERoundPhase::Placing;
    RebuildRoute();
}
void AGemBoard::ApplyPieceVisual(int32 I)
{
    if (!Pieces.IsValidIndex(I)) return;
    if (Pieces[I].bRock)
    {
        Placed[I]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Placed[I]->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_Stone.M_Stone")));
        Placed[I]->SetWorldScale3D(FVector(.90f,.90f,.40f));
        Placed[I]->SetWorldLocation(PlacementPosition(Occupied[I])+FVector(0,0,18));
    }
    else if (const auto* Def=Definition(I))
    {
        UStaticMesh* Model=LoadObject<UStaticMesh>(nullptr,*Def->Model);
        if (!Model) Model=GemModels[Pieces[I].Type*5+FMath::Min(Pieces[I].Quality,4)];
        Placed[I]->SetStaticMesh(Model); Placed[I]->EmptyOverrideMaterials();
        ApplyGemSize(Placed[I]); Placed[I]->SetWorldLocation(PlacementPosition(Occupied[I]));
    }
}
void AGemBoard::MakeRock(int32 I) { Pieces[I].bRock=true; Pieces[I].bPending=false; Pieces[I].Special=NAME_None; ApplyPieceVisual(I); }
bool AGemBoard::KeepSelected()
{
    if (Phase!=ERoundPhase::Choosing || !Pieces.IsValidIndex(Selected) || Pieces[Selected].bRock || !Pieces[Selected].bPending) return false;
    for (int32 I=0;I<Pieces.Num();++I) if (Pieces[I].bPending)
    { if (I==Selected) Pieces[I].bPending=false; else MakeRock(I); }
    StartWave(); return true;
}
bool AGemBoard::CanMerge(int32 Count) const
{
    if (Phase!=ERoundPhase::Choosing || (Count!=2 && Count!=4) || !Pieces.IsValidIndex(Selected)) return false;
    const auto& P=Pieces[Selected]; if (P.bRock || !P.Special.IsNone() || P.Quality+(Count==4?2:1)>4) return false;
    int32 Found=0;
    for (const auto& Other:Pieces) if (!Other.bRock && Other.Special.IsNone() && Other.Type==P.Type && Other.Quality==P.Quality) ++Found;
    return Found>=Count;
}
bool AGemBoard::MergeSelected(int32 Count)
{
    if (!CanMerge(Count)) return false;
    const auto Original=Pieces[Selected]; int32 Needed=Count-1;
    for (int32 I=0;I<Pieces.Num() && Needed>0;++I)
        if (I!=Selected && !Pieces[I].bRock && Pieces[I].Special.IsNone() && Pieces[I].Type==Original.Type && Pieces[I].Quality==Original.Quality) { MakeRock(I); --Needed; }
    Pieces[Selected].Quality+=Count==4?2:1; Pieces[Selected].bPending=true; ApplyPieceVisual(Selected);
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
    Pieces[Selected].Special=FName(*Def->Id); Pieces[Selected].Type=Def->BaseType; Pieces[Selected].Quality=Def->Quality;
    Pieces[Selected].bPending=true; ApplyPieceVisual(Selected);
    return KeepSelected();
}
bool AGemBoard::UpgradeSelectedTower()
{
    if (Phase!=ERoundPhase::Placing && Phase!=ERoundPhase::Choosing) return false;
    const auto* Def=Definition(Selected);
    auto* PC=Cast<AGemController>(GetWorld()->GetFirstPlayerController());
    if (!Def || !PC || Def->Upgrade.IsEmpty() || Def->UpgradeCost<=0 || PC->Gold<Def->UpgradeCost || !Definitions.Contains(Def->Upgrade)) return false;
    PC->Gold-=Def->UpgradeCost; Pieces[Selected].Special=FName(*Def->Upgrade); ApplyPieceVisual(Selected); return true;
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
    Phase=ERoundPhase::Combat; Spawned=0; SpawnTimer=0;
    WaveSize=Waves.IsValidIndex(Wave-1)?Waves[Wave-1].Count:10;
    TowerMana.Init(0,Pieces.Num());
}
void AGemBoard::EndCombat()
{
    if (Lives<=0) { Phase=ERoundPhase::Defeat; return; }
    if (Wave>=Waves.Num()) { Phase=ERoundPhase::Victory; return; }
    ++Wave; OffersPlaced=0; Selected=INDEX_NONE; Phase=ERoundPhase::Placing;
}
void AGemBoard::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (Selected>=0 && Placed.IsValidIndex(Selected))
        DrawDebugBox(GetWorld(),PlacementPosition(Occupied[Selected])+FVector(0,0,25),FVector(49,49,25),FColor::Yellow,false,-1,0,2);
    if (Phase!=ERoundPhase::Combat) return;
    const float DT=FMath::Min(DeltaTime,.1f);
    const auto& Current=Waves[Wave-1]; SpawnTimer-=DT;
    if (Spawned<WaveSize && SpawnTimer<=0)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(this); Mesh->SetupAttachment(RootComponent);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_Enemy.M_Enemy")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent(); Mesh->SetWorldScale3D(FVector(.28f));
        FGemEnemy E; E.Mesh=Mesh; E.Health=E.MaxHealth=Current.Health; E.bFlying=Current.bFlying;
        Mesh->SetWorldLocation(E.bFlying?AirRoute[0]:Route[0]); Enemies.Add(E);
        ++Spawned; SpawnTimer=Current.SpawnInterval;
    }
    for (auto& Area:Areas) Area.Remaining-=DT;
    Areas.RemoveAll([](const FGemAreaEffect& A) { return A.Remaining<=0; });
    for (auto& E:Enemies)
    {
        E.SlowTime=FMath::Max(0.f,E.SlowTime-DT); E.StunTime=FMath::Max(0.f,E.StunTime-DT); E.ArmorTime=FMath::Max(0.f,E.ArmorTime-DT);
        if (E.PoisonTime>0) { E.Health-=E.Poison*DT; E.PoisonTime-=DT; }
        for (const auto& Area:Areas) if (FVector::Dist2D(E.Mesh->GetComponentLocation(),Area.Center)<=Area.Radius) E.Health-=Area.Damage*DT;
        float Speed=Current.Speed*(E.SlowTime>0?1-E.Slow:1);
        for (int32 I=0;I<Pieces.Num();++I)
        {
            if (Pieces[I].bPending) continue;
            const auto* D=Definition(I); if (!D) continue;
            for (const auto& M:D->Modifiers) if (M.Type==TEXT("slow_aura") && FVector::Dist2D(E.Mesh->GetComponentLocation(),Placed[I]->GetComponentLocation())<M.Get(TEXT("radius"))) Speed*=1-M.Get(TEXT("fraction"));
        }
        const auto& Path=E.bFlying?AirRoute:Route;
        float Travel=E.StunTime>0?0:Speed*DT;
        while (Travel>0 && E.RouteIndex<Path.Num())
        {
            const FVector Position=E.Mesh->GetComponentLocation(),Delta=Path[E.RouteIndex]-Position;
            const float Distance=Delta.Size();
            if (Distance<=Travel) { E.Mesh->SetWorldLocation(Path[E.RouteIndex++]); Travel-=Distance; }
            else { E.Mesh->SetWorldLocation(Position+Delta.GetSafeNormal()*Travel); Travel=0; }
        }
        if (E.RouteIndex>=Path.Num() && E.Health>0) { --Lives; E.Health=-1; }
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
        AttackCooldown[I]-=DT;
        const FGemModifier* Mana=nullptr; for (const auto& M:D->Modifiers) if (M.Type==TEXT("mana")) Mana=&M;
        if (Mana) TowerMana[I]=FMath::Min(Mana->Get(TEXT("maximum")),TowerMana[I]+Mana->Get(TEXT("regeneration"))*DT);
        if (AttackCooldown[I]>0) continue;
        TArray<int32> Targets;
        for (int32 N=0;N<Enemies.Num();++N)
            if (Enemies[N].Health>0 && (Enemies[N].bFlying?D->bAir:D->bGround) && FVector::Dist2D(Enemies[N].Mesh->GetComponentLocation(),Placed[I]->GetComponentLocation())<=D->Range) Targets.Add(N);
        if (Targets.IsEmpty()) continue;
        AttackCooldown[I]=D->Interval/(1+Haste);
        for (int32 T=0;T<FMath::Min(D->Targets,Targets.Num());++T)
        {
            auto& E=Enemies[Targets[T]]; float Damage=D->Damage;
            for (int32 Die=0;Die<D->Dice;++Die) Damage+=FMath::RandRange(1,FMath::Max(1,D->Sides));
            Damage*=1+DamageAura;
            for (const auto& M:D->Modifiers)
            {
                if (M.Type==TEXT("critical") && FMath::FRand()<M.Get(TEXT("chance"))) Damage*=M.Get(TEXT("multiplier"),2);
                if (M.Type==TEXT("poison")) { E.Poison=FMath::Max(E.Poison,M.Get(TEXT("damage_per_second"))); E.PoisonTime=M.Get(TEXT("duration")); E.Slow=FMath::Max(E.SlowTime>0?E.Slow:0.f,M.Get(TEXT("slow"))); E.SlowTime=FMath::Max(E.SlowTime,E.PoisonTime); }
                if (M.Type==TEXT("slow")) { E.Slow=FMath::Max(E.SlowTime>0?E.Slow:0.f,M.Get(TEXT("fraction"))); E.SlowTime=FMath::Max(E.SlowTime,M.Get(TEXT("duration"),2)); }
                if (M.Type==TEXT("stun") && FMath::FRand()<M.Get(TEXT("chance"))) E.StunTime=M.Get(TEXT("duration"));
                if (M.Type==TEXT("armor_reduction")) { E.ArmorPenalty=M.Get(TEXT("amount")); E.ArmorTime=M.Get(TEXT("duration")); }
                if (M.Type==TEXT("bonus_gold") && FMath::FRand()<M.Get(TEXT("chance"))) if (auto* PC=Cast<AGemController>(GetWorld()->GetFirstPlayerController())) PC->Gold+=int64(M.Get(TEXT("amount"),1));
                if (M.Type==TEXT("splash") || M.Type==TEXT("splash_slow")) for (auto& Other:Enemies)
                    if (&Other!=&E && Other.bFlying==E.bFlying && FVector::Dist2D(E.Mesh->GetComponentLocation(),Other.Mesh->GetComponentLocation())<M.Get(TEXT("radius")))
                    { if (M.Type==TEXT("splash")) Other.Health-=Damage; else { Other.Slow=M.Get(TEXT("fraction")); Other.SlowTime=M.Get(TEXT("duration"),2); } }
                if (M.Type==TEXT("frost_nova") || M.Type==TEXT("flame_strike"))
                {
                    if (FMath::FRand()<M.Get(TEXT("chance")) && (!Mana || TowerMana[I]>=Mana->Get(TEXT("spell_cost"))))
                    {
                        if (Mana) TowerMana[I]-=Mana->Get(TEXT("spell_cost"));
                        if (M.Type==TEXT("frost_nova")) for (auto& Other:Enemies)
                            if (FVector::Dist2D(E.Mesh->GetComponentLocation(),Other.Mesh->GetComponentLocation())<M.Get(TEXT("radius"))) { Other.Health-=M.Get(TEXT("damage")); Other.Slow=.5f; Other.SlowTime=2; }
                        if (M.Type==TEXT("flame_strike")) { FGemAreaEffect A; A.Center=E.Mesh->GetComponentLocation(); A.Radius=M.Get(TEXT("radius")); A.Damage=M.Get(TEXT("damage_per_second")); A.Remaining=M.Get(TEXT("duration")); Areas.Add(A); }
                    }
                }
                if (M.Type==TEXT("burn")) Damage=M.Get(TEXT("damage_per_second"))*D->Interval;
            }
            float Armor=Current.Armor-(E.ArmorTime>0?E.ArmorPenalty:0);
            for (int32 J=0;J<Pieces.Num();++J)
            {
                if (Pieces[J].bPending) continue;
                const auto* Aura=Definition(J); if (!Aura) continue;
                for (const auto& M:Aura->Modifiers) if (M.Type==TEXT("armor_aura") && M.Get(E.bFlying?TEXT("air"):TEXT("ground"))>0 && FVector::Dist2D(E.Mesh->GetComponentLocation(),Placed[J]->GetComponentLocation())<M.Get(TEXT("radius"))) Armor-=M.Get(TEXT("amount"));
            }
            Damage*=Armor>=0?1/(1+.06f*Armor):2-FMath::Pow(.94f,-Armor);
            E.Health-=Damage;
            DrawDebugLine(GetWorld(),Placed[I]->GetComponentLocation()+FVector(0,0,45),E.Mesh->GetComponentLocation(),FColor::Cyan,false,.12f,0,2);
        }
    }
    for (int32 I=Enemies.Num()-1;I>=0;--I) if (Enemies[I].Health<=0)
    {
        if (Enemies[I].Health!=-1) { Score+=10*Wave; if (auto* PC=Cast<AGemController>(GetWorld()->GetFirstPlayerController())) PC->Gold+=Current.Reward; }
        Enemies[I].Mesh->DestroyComponent(); Enemies.RemoveAt(I);
    }
    if (Lives<=0) { Phase=ERoundPhase::Defeat; return; }
    if (Spawned>=WaveSize && Enemies.IsEmpty()) EndCombat();
}
