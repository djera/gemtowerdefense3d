#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Dom/JsonObject.h"
#include "GemPrototype.generated.h"

UENUM(BlueprintType)
enum class EGroundType : uint8 { Snow, Grass, Road, Intersection, Checkpoint, Entry, Exit };

UENUM(BlueprintType)
enum class ERoundPhase : uint8 { Placing, Choosing, Combat, Victory, Defeat };

USTRUCT(BlueprintType)
struct FGemPiece
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bRock = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bPending = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Level = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float SpeedAura = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float DamageAura = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> TargetIds;
};

USTRUCT()
struct FGemEnemy
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Outline;
    float Health = 0;
    float MaxHealth = 0;
    int32 RouteIndex = 1;
    bool bFlying = false;
    bool bLeaked = false;
    float Slow = 0;
    float SlowTime = 0;
    float Poison = 0;
    float PoisonTime = 0;
    float StunTime = 0;
    float ArmorPenalty = 0;
    float ArmorTime = 0;
    int32 Id = 0, LastHitTower = INDEX_NONE, PoisonTower = INDEX_NONE;
    float PoisonSlow = 0, AuraSlow = 0, AuraArmor = 0, EffectiveSpeed = 0;
};

struct FGemModifier
{
    FString Type;
    FString Channel;
    TMap<FString,float> Values;
    float Get(const FString& Key,float Fallback=0) const { const float* V=Values.Find(Key); return V?*V:Fallback; }
};
struct FTowerDefinition
{
    FString Id,Name,Model,Material,Upgrade,BaseType,Quality,TowerType;
    int32 Dice=1,Sides=1,Targets=1,UpgradeCost=0,InitialLevel=0;
    float Damage=1,Range=300,Interval=1,ProjectileSpeed=1000,DamagePerLevel=.1f;
    bool bGround=true,bAir=true,bInstant=false,bStandard=false;
    TArray<FGemModifier> Modifiers;
    TSharedPtr<FJsonObject> Json;
};
struct FGemCatalogItem { FString Id,Name; bool bMerge=true; };
struct FGemChanceLevel { int32 Cost=0; TMap<FString,int32> Weights; };
struct FGemShot { int32 Tower=INDEX_NONE,EnemyId=0; float Damage=0,Remaining=0; FVector Origin; };
struct FGemRecipe
{
    FString Name,Result;
    TArray<FString> Ingredients;
};
struct FWaveDefinition
{
    int32 Count=10,Reward=5;
    float Health=40,Speed=100,SpawnInterval=.65f,Armor=0;
    bool bFlying=false;
    FString ArmorType=TEXT("Neutral");
    TSharedPtr<FJsonObject> Json;
};
struct FGemAreaEffect
{
    FVector Center;
    float Radius=0,Damage=0,Remaining=0;
    int32 Tower=INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FGroundTile
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FIntPoint Cell = FIntPoint::ZeroValue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EGroundType Type = EGroundType::Grass;
};

USTRUCT(BlueprintType)
struct FGemRoadTile
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FIntPoint Cell=FIntPoint::ZeroValue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Index=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Checkpoint=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bExcluded=false;
};

UCLASS()
class AGemBoard : public AActor
{
    GENERATED_BODY()
public:
    AGemBoard();
    virtual void PostLoad() override;
    virtual void BeginPlay() override;
    virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
    virtual void Tick(float DeltaTime) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grid") TArray<FGroundTile> Tiles;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grid") TArray<FString> LayoutRows;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grid") int32 LandscapeSeed = 4721;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grid") TArray<FIntPoint> CheckpointOrder;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grid") FIntPoint EntryCell = FIntPoint(0,2);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grid") FIntPoint ExitCell = FIntPoint(19,16);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") ERoundPhase Phase = ERoundPhase::Placing;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") int32 Wave = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") int32 Lives = 20;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") int32 Score = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") int32 OffersPlaced = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") int32 Selected = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") int32 SelectedEnemy = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Round") TArray<FGemPiece> Pieces;
    UPROPERTY() TArray<FGemEnemy> Enemies;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Path") TArray<FGemRoadTile> RoadOrder;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path") bool bShowRoadIndices=true;
    UFUNCTION(BlueprintPure) bool RoadIsObstructed(FIntPoint Cell) const;
    UFUNCTION(BlueprintPure) TArray<FIntPoint> DebugRouteCells() const;
    UFUNCTION(BlueprintPure) int32 RequiredRoadCount() const;
    UFUNCTION(BlueprintPure) int32 EnemyRoadTarget(int32 Index) const;
    static constexpr int32 GridSize = 20;
    static constexpr float CellSize = 100.f;
    UFUNCTION(BlueprintPure) bool CanPlace(FIntPoint HalfCell) const;
    UFUNCTION(BlueprintPure) FVector PlacementPosition(FIntPoint HalfCell) const;
    UFUNCTION(BlueprintPure) FIntPoint Snap(FVector World) const;
    UFUNCTION(BlueprintCallable) void Place(FIntPoint HalfCell, int32 Type, int32 Quality = 4);
    UFUNCTION(BlueprintCallable) void PlaceDefinition(FIntPoint HalfCell, FName DefinitionId);
    UFUNCTION(BlueprintCallable, CallInEditor) void ReloadDefinitions();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Definitions") TArray<FString> DefinitionErrors;
    TArray<FGemCatalogItem> GemTypes,Qualities;
    TArray<FGemChanceLevel> ChanceLevels;
    const FTowerDefinition* BaseDefinition(int32 Type,int32 Quality) const;
    const FTowerDefinition* FindDefinition(const FString& Id) const { return Definitions.Find(Id); }
    const FTowerDefinition* MergeResult(int32 Count) const;
    void RunDefinitionChecks(TFunctionRef<void(bool,const TCHAR*)> Check);
    int32 MaxChanceLevel() const { return ChanceLevels.Num()-1; }
    UFUNCTION(BlueprintCallable) void Remove(FIntPoint HalfCell);
    UFUNCTION(BlueprintCallable) void Regenerate();
    UFUNCTION(BlueprintCallable, CallInEditor) void LoadDefaultLayout();
    UFUNCTION(BlueprintCallable) bool KeepSelected();
    UFUNCTION(BlueprintPure) bool CanKeepSelected() const;
    UFUNCTION(BlueprintCallable) bool ClearAllGems();
    UFUNCTION(BlueprintCallable) bool MergeSelected(int32 Count);
    UFUNCTION(BlueprintCallable) bool CraftSelected(int32 Recipe);
    UFUNCTION(BlueprintCallable) void SelectAt(FIntPoint HalfCell);
    UFUNCTION(BlueprintCallable) bool DemolishSelectedRock();
    UFUNCTION(BlueprintPure) bool CanMerge(int32 Count) const;
    UFUNCTION(BlueprintPure) bool CanCraft(int32 Recipe) const;
    UFUNCTION(BlueprintPure) FString SelectedDescription() const;
    UFUNCTION(BlueprintPure) TArray<FString> SelectedInfo() const;
    UFUNCTION(BlueprintCallable) void SelectEnemy(int32 Index);
    bool SelectRay(FVector Origin,FVector Direction);
    UFUNCTION(BlueprintPure) bool HasValidRoute() const;
    UFUNCTION(BlueprintPure) int32 RouteLength() const { return Route.Num(); }
    UFUNCTION(BlueprintCallable) void ResetRun();
    UFUNCTION(BlueprintCallable) bool UpgradeSelectedTower();
    UFUNCTION(BlueprintPure) int32 RecipeCount() const { return Recipes.Num(); }
    UFUNCTION(BlueprintPure) FString RecipeDescription(int32 Index) const;
    const FTowerDefinition* Definition(int32 Index) const;
    int32 PieceAt(FIntPoint HalfCell) const;
    int32 GemCount() const { return Occupied.Num(); }
    void UpdateGhost(FIntPoint HalfCell, int32 Type, bool Visible);
    UPROPERTY() TObjectPtr<class UCameraComponent> Camera;
private:
    UPROPERTY() TArray<TObjectPtr<class UInstancedStaticMeshComponent>> Terrain;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> CheckpointMarkers;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Ghost;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Backdrop;
    UPROPERTY() TObjectPtr<class UMaterialInterface> ValidMaterial;
    UPROPERTY() TObjectPtr<class UMaterialInterface> InvalidMaterial;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Placed;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Outlines;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> RouteMarkers;
    UPROPERTY(Transient) TObjectPtr<class UTexture2D> TerrainBlendMask;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> TerrainBlendInstance;
    TArray<FVector> Route;
    TArray<FVector> AirRoute;
    TArray<int32> RouteRoadIndices,AirRoadIndices;
    TArray<FWaveDefinition> Waves;
    TArray<FGemAreaEffect> Areas;
    TArray<FGemShot> Shots;
    int32 NextEnemyId=1;
    TArray<float> TowerMana;
    TArray<float> AttackCooldown;
    int32 Spawned = 0;
    int32 WaveSize = 0;
    float SpawnTimer = 0;
    TMap<FString,FTowerDefinition> Definitions;
    TSharedPtr<FJsonObject> Catalog;
    TMap<int32,int32> MergeRules;
    FString RockDefinition;
    TArray<FGemRecipe> Recipes;
    void LoadDefinitions();
    void RebuildRoute();
    bool FindRoute(TArray<FVector>& Result, const FIntPoint* ExtraBlock = nullptr, TArray<int32>* RoadIndices=nullptr) const;
    void BuildRoadOrder();
    bool RoadIsBlocked(FIntPoint Cell,const FIntPoint* ExtraBlock) const;
    bool FootprintAvailable(FIntPoint HalfCell) const;
    void MakeRock(int32 Index);
    void StartWave();
    void ApplyPieceVisual(int32 Index);
    class UStaticMeshComponent* CreateOutline(class UStaticMeshComponent* Mesh);
    TArray<int32> FindRecipePieces(int32 Recipe) const;
    void EndCombat();
    TArray<FIntPoint> Occupied;
    void BuildLandscape();
    void ApplyTerrainBlend();
    void ClearGems();
    void ApplyGemSize(class UStaticMeshComponent* Mesh) const;
    void ApplyHit(const FGemShot& Shot);
    void DamageEnemy(FGemEnemy& Enemy,float Damage,int32 Tower,bool ApplyArmor=true);
    void UpdateEnemyAuras(FGemEnemy& Enemy);
};

UCLASS()
class AGemController : public APlayerController
{
    GENERATED_BODY()
public:
    AGemController();
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    UPROPERTY() TObjectPtr<AGemBoard> Board;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<class UGemCameraHandler> CameraHandler;
    UFUNCTION(BlueprintCallable, Category="Camera") void ResetCamera();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy") int64 Gold = 1000000000;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Economy") int32 ChanceLevel = 0;
    UFUNCTION(BlueprintCallable) void UpgradeChance();
    UFUNCTION(BlueprintPure) int32 UpgradeCost() const;
    UFUNCTION(BlueprintPure) int32 QualityChance(int32 Quality) const;
    UFUNCTION(BlueprintCallable) int32 RollQuality();
    FString Status = TEXT("Choose a position to place a gem");
    bool bHover = false;
    bool bRecipesOpen = false;
    bool bShowSelection = false;
    int32 InfoScroll = 0;
    bool bCameraDragging = false;
    bool bHasPreviousMouse = false;
    FVector2D PreviousMouse = FVector2D::ZeroVector;
    FIntPoint Hover;
    static float SidebarWidth(int32 Width) { return FMath::Clamp(Width*.24f,290.f,360.f); }
    static float UIScale(int32 Width,int32 Height) { return FMath::Max(.25f,FMath::Min(1.5f,FMath::Min(Width/1200.f,Height/900.f))); }
};

UCLASS()
class AGemHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};

UCLASS()
class AGemGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AGemGameMode();
};
