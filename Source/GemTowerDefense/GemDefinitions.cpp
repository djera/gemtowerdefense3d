#include "GemPrototype.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

namespace
{
TSharedPtr<FJsonObject> ReadData(const TCHAR* Name)
{
    FString Text; TSharedPtr<FJsonObject> Root;
    if (FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Data")/Name)))
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root);
    return Root;
}
double Number(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double Default=0)
{ double V=Default; if(O) O->TryGetNumberField(K,V); return V; }
FString String(const TSharedPtr<FJsonObject>& O,const TCHAR* K)
{ FString V; if(O) O->TryGetStringField(K,V); return V; }
bool Boolean(const TSharedPtr<FJsonObject>& O,const TCHAR* K,bool Default=true)
{ bool V=Default; if(O) O->TryGetBoolField(K,V); return V; }
TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonObject>& O,const TCHAR* K)
{ const TSharedPtr<FJsonObject>* V=nullptr; return O && O->TryGetObjectField(K,V)?*V:nullptr; }
TArray<TSharedPtr<FJsonValue>> Array(const TSharedPtr<FJsonObject>& O,const TCHAR* K)
{ const TArray<TSharedPtr<FJsonValue>>* V=nullptr; return O && O->TryGetArrayField(K,V)?*V:TArray<TSharedPtr<FJsonValue>>{}; }
void AddWrapped(TArray<FString>& Lines,const FString& Text)
{
    // Fixed-width rows keep even long object paths inside the narrow sidebar.
    const int32 Width=34;
    if(Text.IsEmpty()) { Lines.Add(TEXT("(empty)")); return; }
    for(int32 P=0;P<Text.Len();P+=Width) Lines.Add(Text.Mid(P,Width));
}
void Flatten(TArray<FString>& Lines,const FString& Key,const TSharedPtr<FJsonValue>& Value)
{
    if(!Value) return;
    if(Value->Type==EJson::Object)
    {
        AddWrapped(Lines,Key+TEXT(" {"));
        for(const auto& Pair:Value->AsObject()->Values) Flatten(Lines,FString(*Pair.Key),Pair.Value);
        Lines.Add(TEXT("}"));
    }
    else if(Value->Type==EJson::Array)
    {
        AddWrapped(Lines,Key+TEXT(" [")); int32 I=0;
        for(const auto& Item:Value->AsArray()) Flatten(Lines,FString::FromInt(I++),Item);
        Lines.Add(TEXT("]"));
    }
    else
    {
        FString V;
        if(Value->Type==EJson::String) V=Value->AsString();
        else if(Value->Type==EJson::Boolean) V=Value->AsBool()?TEXT("true"):TEXT("false");
        else if(Value->Type==EJson::Number) V=FString::SanitizeFloat(Value->AsNumber());
        else V=TEXT("null");
        if(Key.Len()+V.Len()+2<=34) Lines.Add(Key+TEXT(": ")+V);
        else { AddWrapped(Lines,Key+TEXT(":")); AddWrapped(Lines,V); }
    }
}
}

void AGemBoard::LoadDefinitions()
{
    Definitions.Reset(); Recipes.Reset(); Waves.Reset(); GemTypes.Reset(); Qualities.Reset();
    ChanceLevels.Reset(); MergeRules.Reset(); DefinitionErrors.Reset();
    const auto Root=ReadData(TEXT("Gems.json")); Catalog=Object(Root,TEXT("catalog"));
    auto Error=[&](FString Message) { DefinitionErrors.AddUnique(Message); };
    if(!Catalog) Error(TEXT("Gems.json: missing or invalid catalog"));
    auto ReadItems=[&](const TCHAR* Key,TArray<FGemCatalogItem>& Items)
    {
        TSet<FString> Ids;
        for(const auto& V:Array(Catalog,Key))
        {
            const auto O=V->AsObject(); FGemCatalogItem Item;
            Item.Id=String(O,TEXT("id")); Item.Name=String(O,TEXT("name")); Item.bMerge=Boolean(O,TEXT("merge_enabled"));
            if(Item.Id.IsEmpty() || Ids.Contains(Item.Id)) Error(FString(Key)+TEXT(": empty/duplicate ID ")+Item.Id);
            Ids.Add(Item.Id); Items.Add(Item);
        }
        if(Items.IsEmpty()) Error(FString(Key)+TEXT(": empty catalog"));
    };
    ReadItems(TEXT("gem_types"),GemTypes); ReadItems(TEXT("qualities"),Qualities);
    RockDefinition=String(Catalog,TEXT("rock_definition"));
    for(const auto& V:Array(Catalog,TEXT("merge_rules")))
    {
        const auto O=V->AsObject(); const int32 Count=Number(O,TEXT("count")),Steps=Number(O,TEXT("quality_steps"));
        if(Count<2 || Steps<1 || MergeRules.Contains(Count)) Error(TEXT("Invalid/duplicate merge rule"));
        MergeRules.Add(Count,Steps);
    }
    for(const auto& V:Array(Catalog,TEXT("chance_levels")))
    {
        const auto O=V->AsObject(),Weights=Object(O,TEXT("weights")); FGemChanceLevel Level;
        Level.Cost=Number(O,TEXT("upgrade_cost")); int32 Total=0;
        if(Number(O,TEXT("level"),-1)!=ChanceLevels.Num() || Level.Cost<0) Error(TEXT("Invalid chance level/cost"));
        if(Weights) for(const auto& Pair:Weights->Values)
        {
            const double Weight=Pair.Value->AsNumber();
            if(Weight<0 || Weight!=FMath::FloorToDouble(Weight) || !Qualities.ContainsByPredicate([&](const auto& Q){return Q.Id==FString(*Pair.Key);})) Error(TEXT("Invalid chance weight: ")+FString(*Pair.Key));
            Level.Weights.Add(FString(*Pair.Key),int32(Weight)); Total+=int32(Weight);
        }
        if(Total!=100) Error(TEXT("Chance weights must sum to 100"));
        ChanceLevels.Add(Level);
    }
    if(ChanceLevels.IsEmpty()) Error(TEXT("No chance levels defined"));
    const TSet<FString> Handlers={TEXT("critical"),TEXT("poison"),TEXT("splash"),TEXT("slow"),TEXT("splash_slow"),TEXT("attack_speed_aura"),TEXT("damage_aura"),TEXT("slow_aura"),TEXT("armor_aura"),TEXT("stun"),TEXT("armor_reduction"),TEXT("bonus_gold"),TEXT("mana"),TEXT("frost_nova"),TEXT("flame_strike")};
    const TMap<FString,FString> RequiredParameters={
        {TEXT("critical"),TEXT("chance multiplier")},{TEXT("poison"),TEXT("damage_per_second duration slow")},
        {TEXT("splash"),TEXT("radius")},{TEXT("slow"),TEXT("fraction duration")},{TEXT("splash_slow"),TEXT("radius fraction duration")},
        {TEXT("attack_speed_aura"),TEXT("fraction radius channel")},{TEXT("damage_aura"),TEXT("fraction radius channel")},
        {TEXT("slow_aura"),TEXT("fraction radius ground air channel")},{TEXT("armor_aura"),TEXT("amount radius ground air channel")},
        {TEXT("stun"),TEXT("chance duration")},{TEXT("armor_reduction"),TEXT("amount duration")},{TEXT("bonus_gold"),TEXT("chance")},
        {TEXT("mana"),TEXT("maximum regeneration spell_cost")},{TEXT("frost_nova"),TEXT("chance damage radius fraction duration")},
        {TEXT("flame_strike"),TEXT("chance damage_per_second radius duration")}};
    TSet<FString> TowerTypes;
    for(const auto& V:Array(Catalog,TEXT("tower_types"))) TowerTypes.Add(String(V->AsObject(),TEXT("id")));
    for(const TCHAR* Group:{TEXT("base_gems"),TEXT("special_towers"),TEXT("obstacles")})
        for(const auto& Entry:Array(Root,Group))
        {
            const auto O=Entry->AsObject(),Stats=Object(O,TEXT("stats")); FTowerDefinition D; D.Json=O;
            D.Id=String(O,TEXT("id")); D.Name=String(O,TEXT("name")); D.Model=String(O,TEXT("model")); D.Material=String(O,TEXT("material"));
            D.BaseType=String(O,TEXT("gem_type")); D.Quality=String(O,TEXT("quality")); D.TowerType=String(O,TEXT("tower_type"));
            D.bStandard=FString(Group)==TEXT("base_gems");
            D.Damage=Number(Stats,TEXT("damage_base")); D.Dice=Number(Stats,TEXT("damage_dice")); D.Sides=Number(Stats,TEXT("damage_sides"));
            D.Range=Number(Stats,TEXT("range")); D.Interval=Number(Stats,TEXT("attack_interval")); D.Targets=Number(Stats,TEXT("targets"));
            D.ProjectileSpeed=Number(Stats,TEXT("projectile_speed")); D.bInstant=String(Stats,TEXT("delivery"))==TEXT("instant");
            D.InitialLevel=Number(Stats,TEXT("initial_level")); D.DamagePerLevel=Number(Stats,TEXT("damage_per_level"),.1);
            D.bGround=Boolean(Stats,TEXT("attacks_ground")); D.bAir=Boolean(Stats,TEXT("attacks_air"));
            D.Upgrade=String(O,TEXT("upgrades_to")); D.UpgradeCost=Number(O,TEXT("upgrade_cost"));
            if(D.Id.IsEmpty() || Definitions.Contains(D.Id)) Error(TEXT("Empty/duplicate tower ID: ")+D.Id);
            if(!TowerTypes.Contains(D.TowerType)) Error(D.Id+TEXT(": unknown tower_type"));
            if(!Stats || D.Interval<=0 || D.Range<0 || D.Damage<0 || D.Dice<0 || (D.Dice>0 && D.Sides<1) || D.Targets<0 || (!D.bInstant && D.ProjectileSpeed<=0)) Error(D.Id+TEXT(": invalid attack stats"));
            if(D.Id!=RockDefinition && (!GemTypes.ContainsByPredicate([&](const auto& T){return T.Id==D.BaseType;}) || !Qualities.ContainsByPredicate([&](const auto& Q){return Q.Id==D.Quality;}))) Error(D.Id+TEXT(": unknown gem type/quality"));
            if(!LoadObject<UStaticMesh>(nullptr,*D.Model)) Error(D.Id+TEXT(": missing model ")+D.Model);
            if(!LoadObject<UMaterialInterface>(nullptr,*D.Material)) Error(D.Id+TEXT(": missing material ")+D.Material);
            for(const auto& V:Array(O,TEXT("modifiers")))
            {
                const auto M=V->AsObject(); if(!M) { Error(D.Id+TEXT(": invalid modifier")); continue; }
                FGemModifier Mod; Mod.Type=String(M,TEXT("type")); Mod.Channel=String(M,TEXT("channel"));
                if(!Handlers.Contains(Mod.Type)) Error(D.Id+TEXT(": unsupported modifier ")+Mod.Type);
                TArray<FString> Required; RequiredParameters.FindRef(Mod.Type).ParseIntoArrayWS(Required);
                for(const auto& Key:Required) if(!M->HasField(Key)) Error(D.Id+TEXT(" / ")+Mod.Type+TEXT(": missing ")+Key);
                for(const auto& Pair:M->Values)
                {
                    double N; bool B;
                    if(Pair.Value->TryGetNumber(N))
                    {
                        const FString Key(*Pair.Key);
                        if(!FMath::IsFinite(N) || N<0 || ((Key==TEXT("chance") || Key==TEXT("fraction") || Key==TEXT("slow")) && N>1)) Error(D.Id+TEXT(": invalid modifier value ")+Key);
                        Mod.Values.Add(Key,float(N));
                    }
                    else if(Pair.Value->TryGetBool(B)) Mod.Values.Add(FString(*Pair.Key),B?1.f:0.f);
                }
                D.Modifiers.Add(Mod);
            }
            Definitions.Add(D.Id,MoveTemp(D));
        }
    if(!Definitions.Contains(RockDefinition)) Error(TEXT("Missing rock definition"));
    for(int32 T=0;T<GemTypes.Num();++T) for(int32 Q=0;Q<Qualities.Num();++Q)
    {
        int32 Count=0;
        for(const auto& Pair:Definitions) if(Pair.Value.bStandard && Pair.Value.BaseType==GemTypes[T].Id && Pair.Value.Quality==Qualities[Q].Id) ++Count;
        if(Count!=1) Error(TEXT("Expected one base definition for ")+GemTypes[T].Id+TEXT(" / ")+Qualities[Q].Id);
    }
    for(const auto& Pair:Definitions)
    {
        const auto& D=Pair.Value;
        if(D.UpgradeCost<0 || (!D.Upgrade.IsEmpty() && (!Definitions.Contains(D.Upgrade) || D.UpgradeCost<=0))) Error(D.Id+TEXT(": invalid upgrade target/cost"));
        TSet<FString> Seen; const FTowerDefinition* Next=&D;
        while(Next && !Next->Upgrade.IsEmpty())
        { if(Seen.Contains(Next->Id)) { Error(D.Id+TEXT(": upgrade cycle")); break; } Seen.Add(Next->Id); Next=Definitions.Find(Next->Upgrade); }
    }
    const auto RecipeRoot=ReadData(TEXT("Recipes.json"));
    if(!RecipeRoot) Error(TEXT("Missing/invalid Recipes.json"));
    for(const auto& V:Array(RecipeRoot,TEXT("recipes")))
    {
        const auto O=V->AsObject(); FGemRecipe R; R.Name=String(O,TEXT("name")); R.Result=String(O,TEXT("result"));
        if(!Definitions.Contains(R.Result)) Error(TEXT("Recipe result missing: ")+R.Result);
        for(const auto& Ingredient:Array(O,TEXT("ingredients")))
        {
            const auto Item=Ingredient->AsObject(); const FString Id=String(Item,TEXT("id")); const int32 Count=Number(Item,TEXT("count"),1);
            if(!Definitions.Contains(Id) || Count<1) Error(TEXT("Invalid recipe ingredient: ")+Id);
            for(int32 I=0;I<Count && I<100;++I) R.Ingredients.Add(Id);
        }
        if(R.Ingredients.IsEmpty()) Error(TEXT("Empty recipe: ")+R.Name);
        Recipes.Add(R);
    }
    const auto WaveRoot=ReadData(TEXT("Waves.json"));
    TSet<FString> ArmorTypes;
    for(const auto& V:Array(Catalog,TEXT("armor_types"))) ArmorTypes.Add(V->AsString());
    const auto DamageTable=Object(Catalog,TEXT("damage_table"));
    for(const auto& Type:GemTypes)
    {
        const auto Row=Object(DamageTable,*Type.Id);
        for(const auto& Armor:ArmorTypes) if(!Row || Number(Row,*Armor,-1)<0) Error(Type.Id+TEXT(": missing armor multiplier ")+Armor);
    }
    if(ArmorTypes.IsEmpty() || Array(Catalog,TEXT("armor_positive")).IsEmpty() || Array(Catalog,TEXT("armor_negative")).IsEmpty()) Error(TEXT("Missing armor definitions"));
    for(const auto& V:Array(WaveRoot,TEXT("waves")))
    {
        const auto O=V->AsObject(); FWaveDefinition W; W.Json=O;
        W.Count=Number(O,TEXT("count"),10); W.Health=Number(O,TEXT("health"),40); W.Speed=Number(O,TEXT("speed"),100);
        W.SpawnInterval=Number(O,TEXT("spawn_interval"),.65); W.Reward=Number(O,TEXT("reward"),5); W.Armor=Number(O,TEXT("armor"));
        W.bFlying=Boolean(O,TEXT("flying"),false); const auto A=String(O,TEXT("armor_type")); if(!A.IsEmpty()) W.ArmorType=A;
        if(!ArmorTypes.Contains(W.ArmorType)) Error(TEXT("Unknown wave armor type: ")+W.ArmorType);
        if(W.Count<1 || W.Health<=0 || W.SpawnInterval<=0) Error(TEXT("Invalid wave values"));
        Waves.Add(W);
    }
    if(Waves.IsEmpty()) Error(TEXT("Missing/empty Waves.json"));
    for(const auto& Message:DefinitionErrors) UE_LOG(LogTemp,Error,TEXT("GEM_DATA: %s"),*Message);
    UE_LOG(LogTemp,Display,TEXT("GEM_DATA: %d definitions, %d recipes, %d errors"),Definitions.Num(),Recipes.Num(),DefinitionErrors.Num());
}

void AGemBoard::ReloadDefinitions()
{
    // Reload only on an empty build board, so live references cannot disappear.
    if(!Pieces.IsEmpty() || Phase!=ERoundPhase::Placing) return;
    LoadDefinitions();
}
const FTowerDefinition* AGemBoard::BaseDefinition(int32 Type,int32 Quality) const
{
    if(!GemTypes.IsValidIndex(Type) || !Qualities.IsValidIndex(Quality)) return nullptr;
    for(const auto& Pair:Definitions) if(Pair.Value.bStandard && Pair.Value.BaseType==GemTypes[Type].Id && Pair.Value.Quality==Qualities[Quality].Id) return &Pair.Value;
    return nullptr;
}
const FTowerDefinition* AGemBoard::Definition(int32 Index) const
{ return Pieces.IsValidIndex(Index) && !Pieces[Index].bRock?Definitions.Find(Pieces[Index].DefinitionId.ToString()):nullptr; }
const FTowerDefinition* AGemBoard::MergeResult(int32 Count) const
{
    const auto* D=Definition(Selected); const auto* Steps=MergeRules.Find(Count);
    if(!D || !D->bStandard || !Steps) return nullptr;
    const int32 Q=Qualities.IndexOfByPredicate([&](const auto& Item){return Item.Id==D->Quality;})+*Steps;
    const int32 T=GemTypes.IndexOfByPredicate([&](const auto& Item){return Item.Id==D->BaseType;});
    return Qualities.IsValidIndex(Q) && Qualities[Q].bMerge?BaseDefinition(T,Q):nullptr;
}

TArray<FString> AGemBoard::SelectedInfo() const
{
    TArray<FString> Lines;
    if(!DefinitionErrors.IsEmpty())
    { Lines.Add(TEXT("DEFINITION ERRORS")); for(const auto& E:DefinitionErrors) AddWrapped(Lines,E); }
    if(Enemies.IsValidIndex(SelectedEnemy))
    {
        const auto& E=Enemies[SelectedEnemy];
        Lines.Add(E.bFlying?TEXT("Flying enemy / live variables"):TEXT("Ground enemy / live variables"));
        auto F=[&](const TCHAR* Key,float V){AddWrapped(Lines,FString(Key)+TEXT(": ")+FString::SanitizeFloat(V));};
        F(TEXT("Id"),E.Id); F(TEXT("Health"),E.Health); F(TEXT("MaxHealth"),E.MaxHealth); F(TEXT("RouteIndex"),E.RouteIndex);
        F(TEXT("Next required road index"),EnemyRoadTarget(SelectedEnemy)); F(TEXT("Total road tiles"),RoadOrder.Num());
        F(TEXT("bFlying"),E.bFlying); F(TEXT("bLeaked"),E.bLeaked); F(TEXT("Slow"),E.Slow); F(TEXT("SlowTime"),E.SlowTime);
        F(TEXT("Poison"),E.Poison); F(TEXT("PoisonSlow"),E.PoisonSlow); F(TEXT("PoisonTime"),E.PoisonTime); F(TEXT("StunTime"),E.StunTime);
        F(TEXT("ArmorPenalty"),E.ArmorPenalty); F(TEXT("ArmorTime"),E.ArmorTime); F(TEXT("AuraSlow"),E.AuraSlow); F(TEXT("AuraArmor"),E.AuraArmor);
        F(TEXT("EffectiveSpeed"),E.EffectiveSpeed); F(TEXT("LastHitTower"),E.LastHitTower); F(TEXT("PoisonTower"),E.PoisonTower);
        F(TEXT("Damage on exit (lives)"),1); F(TEXT("Attack range"),0);
        AddWrapped(Lines,TEXT("Position: ")+E.Mesh->GetComponentLocation().ToString());
        AddWrapped(Lines,TEXT("Mesh: ")+E.Mesh->GetPathName()); AddWrapped(Lines,TEXT("Outline: ")+GetPathNameSafe(E.Outline));
        if(Waves.IsValidIndex(Wave-1)) { AddWrapped(Lines,TEXT("ArmorType: ")+Waves[Wave-1].ArmorType); Flatten(Lines,TEXT("Wave definition"),MakeShared<FJsonValueObject>(Waves[Wave-1].Json)); }
        return Lines;
    }
    if(!Pieces.IsValidIndex(Selected)) { Lines.Add(TEXT("Select a gem, stone or enemy")); return Lines; }
    const auto& P=Pieces[Selected]; const auto* D=Definitions.Find(P.DefinitionId.ToString());
    if(D)
    {
        Lines.Add(D->Name);
        Lines.Add(FString::Printf(TEXT("Damage: %.0f - %.0f"),D->Damage+D->Dice,D->Damage+D->Dice*D->Sides));
        Lines.Add(FString::Printf(TEXT("Range: %.1f tiles / %.1f cm"),D->Range/CellSize,D->Range));
        Lines.Add(FString::Printf(TEXT("Attack interval: %.3f s"),D->Interval));
    }
    Lines.Add(TEXT("LIVE VARIABLES"));
    for(TFieldIterator<FProperty> It(FGemPiece::StaticStruct());It;++It)
    {
        FString V; It->ExportTextItem_Direct(V,It->ContainerPtrToValuePtr<void>(&P),nullptr,nullptr,PPF_None);
        AddWrapped(Lines,It->GetName()+TEXT(": ")+V);
    }
    AddWrapped(Lines,TEXT("selected: true")); AddWrapped(Lines,TEXT("tile (half cells): ")+Occupied[Selected].ToString());
    AddWrapped(Lines,TEXT("Position: ")+Placed[Selected]->GetComponentLocation().ToString());
    AddWrapped(Lines,TEXT("towerSprite / model: ")+GetPathNameSafe(Placed[Selected]->GetStaticMesh()));
    AddWrapped(Lines,TEXT("Cooldown remaining: ")+FString::SanitizeFloat(AttackCooldown[Selected]));
    AddWrapped(Lines,TEXT("Mana: ")+FString::SanitizeFloat(TowerMana.IsValidIndex(Selected)?TowerMana[Selected]:0));
    Lines.Add(FString::Printf(TEXT("Pending projectiles: %d"),Shots.FilterByPredicate([&](const auto& S){return S.Tower==Selected;}).Num()));
    if(D)
    {
        Lines.Add(TEXT("EDITABLE DEFINITION"));
        for(const auto& Pair:D->Json->Values) if(FString(*Pair.Key)!=TEXT("source_properties")) Flatten(Lines,FString(*Pair.Key),Pair.Value);
        const auto Source=Object(D->Json,TEXT("source_properties"));
        if(Source) { Lines.Add(TEXT("SOURCE VALUES (reference only)")); Flatten(Lines,TEXT("Tower.m"),MakeShared<FJsonValueObject>(Source)); }
        for(const auto& R:Recipes) if(R.Result==D->Id || R.Ingredients.Contains(D->Id))
        { AddWrapped(Lines,TEXT("Recipe: ")+R.Name); for(const auto& Id:R.Ingredients) AddWrapped(Lines,Id); AddWrapped(Lines,TEXT("=> ")+R.Result); }
    }
    return Lines;
}

