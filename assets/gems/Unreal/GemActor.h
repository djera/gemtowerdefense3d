#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GemActor.generated.h"

UENUM(BlueprintType)
enum class EGemType : uint8 { Amethyst, Aquamarine, Diamond, Emerald, Opal, Ruby, Sapphire, Topaz };

UENUM(BlueprintType)
enum class EGemQuality : uint8 { Chipped, Flawed, Normal, Flawless, Perfect };

UCLASS(Blueprintable)
class AGemActor : public AActor
{
    GENERATED_BODY()
public:
    AGemActor();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gem")
    EGemType Type = EGemType::Amethyst;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gem")
    EGemQuality Quality = EGemQuality::Normal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gem")
    TObjectPtr<class UStaticMeshComponent> GemMesh;
    UFUNCTION(BlueprintCallable, Category="Gem")
    void SetGem(EGemType NewType, EGemQuality NewQuality);
    virtual void OnConstruction(const FTransform& Transform) override;
private:
    // Hard references keep the complete library available in packaged games.
    UPROPERTY()
    TArray<TObjectPtr<class UStaticMesh>> Library;
    void RefreshGem();
};
