#include "GemPrototype.h"
#include "Components/StaticMeshComponent.h"

// Integration checks run only with the existing explicit -GemSmokeTest flag.
void AGemBoard::RunDefinitionChecks(TFunctionRef<void(bool,const TCHAR*)> Check)
{
    Check(DefinitionErrors.IsEmpty() && Definitions.Num()==81,TEXT("All imported definitions, models, materials and links resolve"));
    auto Offer=[&](const FString& Id,int32 I){ PlaceDefinition(FIntPoint(8+I*4,14),FName(*Id)); };
    bool Inspects=true;
    for(const auto& Pair:Definitions)
    {
        if(Pair.Key==RockDefinition) continue;
        ResetRun(); Offer(Pair.Key,0); Selected=0;
        Inspects &= Pieces.Num()==1 && Definition(0) && Definition(0)->Id==Pair.Key && SelectedInfo().Contains(TEXT("EDITABLE DEFINITION"));
        if(Pieces.Num()==1) Inspects &= Placed[0]->GetStaticMesh() && Placed[0]->GetMaterial(0);
    }
    Check(Inspects,TEXT("Every imported tower places by string ID and exposes its complete definition"));
    bool Crafted=true;
    for(int32 R=0;R<Recipes.Num();++R)
    {
        ResetRun(); int32 I=0; for(const auto& Id:Recipes[R].Ingredients) Offer(Id,I++);
        while(I<5) Offer(BaseDefinition(0,0)->Id,I++);
        Selected=0; Crafted &= CanCraft(R) && CraftSelected(R) && Definition(0) && Definition(0)->Id==Recipes[R].Result;
    }
    Check(Crafted,TEXT("All 13 recipes craft the intended result using their defined ingredients"));
    auto* PC=Cast<AGemController>(GetWorld()->GetFirstPlayerController());
    const int64 Gold=PC?PC->Gold:0;
    bool Upgraded=true;
    for(const auto& Pair:Definitions) if(!Pair.Value.Upgrade.IsEmpty())
    {
        ResetRun(); Offer(Pair.Key,0); Selected=0; const int64 Before=PC?PC->Gold:0;
        Upgraded &= UpgradeSelectedTower() && Definition(0)->Id==Pair.Value.Upgrade && PC && PC->Gold==Before-Pair.Value.UpgradeCost;
    }
    if(PC) PC->Gold=Gold;
    Check(Upgraded,TEXT("All special upgrade links change definition and deduct the exact price"));
    bool Great=true;
    for(int32 T=0;T<GemTypes.Num();++T)
    {
        ResetRun(); for(int32 I=0;I<5;++I) Offer(BaseDefinition(T,4)->Id,I); Selected=0;
        Great &= MergeSelected(2) && Definition(0)->Quality==TEXT("Great");
        ResetRun(); for(int32 I=0;I<5;++I) Offer(BaseDefinition(T,3)->Id,I); Selected=0;
        Great &= MergeSelected(4) && Definition(0)->Quality==TEXT("Great");
        ResetRun(); for(int32 I=0;I<5;++I) Offer(BaseDefinition(T,5)->Id,I); Selected=0;
        Great &= !CanMerge(2) && !CanMerge(4);
    }
    Check(Great,TEXT("All eight types merge to Great and cannot exceed it"));
    if(PC)
    {
        const int32 Old=PC->ChanceLevel; bool Rolls=true;
        for(int32 L=0;L<ChanceLevels.Num();++L)
        {
            PC->ChanceLevel=L; int32 Sum=0;
            for(int32 Q=0;Q<Qualities.Num();++Q) Sum+=PC->QualityChance(Q);
            Rolls &= Sum==100 && PC->QualityChance(5)==0;
            for(int32 I=0;I<1000;++I) { const int32 Q=PC->RollQuality(); Rolls &= Qualities.IsValidIndex(Q) && PC->QualityChance(Q)>0; }
        }
        PC->ChanceLevel=Old; Check(Rolls,TEXT("Every chance level totals 100 and rolls only enabled qualities, never Great"));
    }
    ResetRun(); Offer(TEXT("Aquamarine_Normal"),0);
    for(int32 I=1;I<5;++I) Offer(BaseDefinition(0,0)->Id,I);
    Selected=0; KeepSelected(); Tick(.001f);
    if(!Enemies.IsEmpty())
    {
        auto& E=Enemies[0]; E.Health=E.MaxHealth=10000; E.Mesh->SetWorldLocation(Placed[0]->GetComponentLocation()+FVector(80,0,20));
        Tick(.001f); Check(!Shots.IsEmpty() && E.Health==10000,TEXT("Projectile attacks wait for their data-defined travel time"));
        for(int32 I=0;I<4;++I) Tick(.1f);
        Check(!Enemies.IsEmpty() && Enemies[0].Health<10000,TEXT("Projectiles hit the stable enemy ID after travel"));
    }
    else Check(false,TEXT("Projectile test enemy spawned"));
    auto SetupHit=[&](const TCHAR* Id)
    {
        ResetRun(); Offer(Id,0); Pieces[0].bPending=false; TowerMana.Init(100,1);
        FGemEnemy E; E.Id=NextEnemyId++; E.Health=E.MaxHealth=10000;
        E.Mesh=NewObject<UStaticMeshComponent>(this); E.Mesh->SetupAttachment(RootComponent); E.Mesh->RegisterComponent();
        E.Mesh->SetWorldLocation(Placed[0]->GetComponentLocation()+FVector(50,0,0)); Enemies.Add(E);
        FGemShot S; S.Tower=0; S.EnemyId=E.Id; S.Damage=100; ApplyHit(S);
    };
    SetupHit(TEXT("Jade"));
    Check(Enemies[0].Poison==5 && Enemies[0].PoisonSlow==.5f && Enemies[0].PoisonTime==2,TEXT("Repaired Jade poison uses editable damage, slow and duration"));
    SetupHit(TEXT("Silver"));
    Check(Enemies[0].Slow==.2f && Enemies[0].SlowTime==5,TEXT("Ice slow uses this source's five-second duration"));
    SetupHit(TEXT("Gold"));
    Check(Enemies[0].ArmorPenalty==5 && Enemies[0].ArmorTime==5,TEXT("Armor reduction is applied on impact"));
    SetupHit(TEXT("Uranium238")); UpdateEnemyAuras(Enemies[0]);
    Check(Enemies[0].AuraSlow==.5f && Enemies[0].AuraArmor==0,TEXT("Repaired Uranium slows rather than changing armor"));
    auto* Paraiba=Definitions.Find(TEXT("Paraiba"));
    for(auto& M:Paraiba->Modifiers) if(M.Type==TEXT("frost_nova")) M.Values[TEXT("chance")]=1;
    SetupHit(TEXT("Paraiba"));
    Check(TowerMana[0]==95 && Enemies[0].Health<=9700 && Enemies[0].SlowTime==5,TEXT("Completed Paraiba frost nova consumes mana and applies its configured damage and slow"));
    for(auto& M:Paraiba->Modifiers) if(M.Type==TEXT("frost_nova")) M.Values[TEXT("chance")]=.2f;
    auto* D=Definitions.Find(TEXT("Aquamarine_Chipped")); D->Json->SetStringField(TEXT("custom_inspector_probe"),TEXT("custom value shown"));
    ResetRun(); Offer(D->Id,0); Selected=0;
    Check(SelectedInfo().Contains(TEXT("custom value shown")),TEXT("The inspector automatically includes new custom JSON fields"));
    D->Json->RemoveField(TEXT("custom_inspector_probe")); ResetRun();
}
