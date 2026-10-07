#include "GemPrototype.h"
#include "Components/StaticMeshComponent.h"

void AGemBoard::UpdateEnemyAuras(FGemEnemy& E)
{
    TMap<FString,float> Slow,Armor;
    for(int32 I=0;I<Pieces.Num();++I)
    {
        const auto* D=Definition(I); if(!D || Pieces[I].bPending) continue;
        for(const auto& M:D->Modifiers)
        {
            if(M.Get(E.bFlying?TEXT("air"):TEXT("ground"))<=0 || FVector::Dist2D(E.Mesh->GetComponentLocation(),Placed[I]->GetComponentLocation())>M.Get(TEXT("radius"))) continue;
            if(M.Type==TEXT("slow_aura")) Slow.FindOrAdd(M.Channel)=FMath::Max(Slow.FindRef(M.Channel),M.Get(TEXT("fraction")));
            if(M.Type==TEXT("armor_aura")) Armor.FindOrAdd(M.Channel)=FMath::Max(Armor.FindRef(M.Channel),M.Get(TEXT("amount")));
        }
    }
    E.AuraSlow=0; E.AuraArmor=0;
    for(const auto& V:Slow) E.AuraSlow+=V.Value;
    for(const auto& V:Armor) E.AuraArmor+=V.Value;
    E.AuraSlow=FMath::Clamp(E.AuraSlow,0.f,1.f);
}

void AGemBoard::DamageEnemy(FGemEnemy& E,float Damage,int32 Tower,bool ApplyArmor)
{
    if(E.Health<=0 || E.bLeaked || Damage<=0) return;
    const auto* D=Definition(Tower);
    if(ApplyArmor && Catalog && Waves.IsValidIndex(Wave-1))
    {
        const auto& W=Waves[Wave-1];
        const TSharedPtr<FJsonObject>* Table=nullptr;
        if(D && Catalog->TryGetObjectField(TEXT("damage_table"),Table))
        {
            const TSharedPtr<FJsonObject>* Row=nullptr; double Multiplier=1;
            if((*Table)->TryGetObjectField(D->BaseType,Row)) (*Row)->TryGetNumberField(W.ArmorType,Multiplier);
            Damage*=Multiplier;
        }
        const float Armor=W.Armor-(E.ArmorTime>0?E.ArmorPenalty:0)-E.AuraArmor;
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if(Catalog->TryGetArrayField(Armor>=0?TEXT("armor_positive"):TEXT("armor_negative"),Values) && !Values->IsEmpty())
        {
            float Index=FMath::Abs(Armor);
            if(Armor<0) Index=FMath::Min(Index,float(Values->Num()-1));
            const int32 Low=FMath::FloorToInt(Index),High=FMath::CeilToInt(Index);
            Damage*=Values->IsValidIndex(High)?FMath::Lerp(float((*Values)[Low]->AsNumber()),float((*Values)[High]->AsNumber()),Index-Low):1/(1+.06f*Armor);
        }
    }
    E.Health-=Damage; E.LastHitTower=Tower;
    if(E.Health<=0 && Pieces.IsValidIndex(Tower)) ++Pieces[Tower].Kills;
}

void AGemBoard::ApplyHit(const FGemShot& Shot)
{
    const auto* D=Definition(Shot.Tower);
    auto* Enemy=Enemies.FindByPredicate([&](const auto& E){return E.Id==Shot.EnemyId;});
    if(!D || !Enemy || Enemy->Health<=0 || Enemy->bLeaked) return;
    auto& E=*Enemy;
    const FGemModifier* Mana=nullptr;
    for(const auto& M:D->Modifiers) if(M.Type==TEXT("mana")) Mana=&M;
    // Status effects are independent: a combined tower can apply poison and stun.
    for(const auto& M:D->Modifiers)
    {
        if(M.Type==TEXT("poison"))
        {
            E.Poison=FMath::Max(E.PoisonTime>0?E.Poison:0.f,M.Get(TEXT("damage_per_second")));
            E.PoisonSlow=FMath::Max(E.PoisonTime>0?E.PoisonSlow:0.f,M.Get(TEXT("slow")));
            E.PoisonTime=FMath::Max(E.PoisonTime,M.Get(TEXT("duration"))); E.PoisonTower=Shot.Tower;
        }
        if(M.Type==TEXT("slow")) { E.Slow=FMath::Max(E.SlowTime>0?E.Slow:0.f,M.Get(TEXT("fraction"))); E.SlowTime=FMath::Max(E.SlowTime,M.Get(TEXT("duration"))); }
        if(M.Type==TEXT("stun") && FMath::FRand()<M.Get(TEXT("chance"))) E.StunTime=FMath::Max(E.StunTime,M.Get(TEXT("duration")));
        if(M.Type==TEXT("armor_reduction")) { E.ArmorPenalty=FMath::Max(E.ArmorTime>0?E.ArmorPenalty:0.f,M.Get(TEXT("amount"))); E.ArmorTime=FMath::Max(E.ArmorTime,M.Get(TEXT("duration"))); }
        if(M.Type==TEXT("bonus_gold") && FMath::FRand()<M.Get(TEXT("chance")))
            if(auto* PC=Cast<AGemController>(GetWorld()->GetFirstPlayerController())) PC->Gold+=int64(M.Get(TEXT("amount"))+Wave*M.Get(TEXT("wave_fraction")));
        if(M.Type==TEXT("splash") || M.Type==TEXT("splash_slow")) for(auto& Other:Enemies)
        {
            if(&Other==&E || Other.bFlying!=E.bFlying || Other.Health<=0 || FVector::Dist2D(E.Mesh->GetComponentLocation(),Other.Mesh->GetComponentLocation())>M.Get(TEXT("radius"))) continue;
            if(M.Type==TEXT("splash_slow")) { Other.Slow=FMath::Max(Other.SlowTime>0?Other.Slow:0.f,M.Get(TEXT("fraction"))); Other.SlowTime=FMath::Max(Other.SlowTime,M.Get(TEXT("duration"))); }
            DamageEnemy(Other,Shot.Damage,Shot.Tower);
        }
        if(M.Type==TEXT("frost_nova") || M.Type==TEXT("flame_strike"))
        {
            if(FMath::FRand()>=M.Get(TEXT("chance")) || (Mana && (!TowerMana.IsValidIndex(Shot.Tower) || TowerMana[Shot.Tower]<Mana->Get(TEXT("spell_cost"))))) continue;
            if(Mana) TowerMana[Shot.Tower]-=Mana->Get(TEXT("spell_cost"));
            if(M.Type==TEXT("frost_nova")) for(auto& Other:Enemies)
                if(Other.Health>0 && Other.bFlying==E.bFlying && FVector::Dist2D(E.Mesh->GetComponentLocation(),Other.Mesh->GetComponentLocation())<=M.Get(TEXT("radius")))
                { DamageEnemy(Other,M.Get(TEXT("damage")),Shot.Tower); Other.Slow=FMath::Max(Other.SlowTime>0?Other.Slow:0.f,M.Get(TEXT("fraction"))); Other.SlowTime=FMath::Max(Other.SlowTime,M.Get(TEXT("duration"))); }
            if(M.Type==TEXT("flame_strike"))
            { FGemAreaEffect A; A.Center=E.Mesh->GetComponentLocation(); A.Radius=M.Get(TEXT("radius")); A.Damage=M.Get(TEXT("damage_per_second")); A.Remaining=M.Get(TEXT("duration")); A.Tower=Shot.Tower; Areas.Add(A); }
        }
    }
    DamageEnemy(E,Shot.Damage,Shot.Tower);
}
