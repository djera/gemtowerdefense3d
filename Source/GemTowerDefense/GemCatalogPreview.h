#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GemCatalogPreview.generated.h"

/** A local, collision-free model shown only by the catalog's capture camera. */
UCLASS(NotBlueprintable, Transient)
class AGemCatalogPreview : public AActor
{
    GENERATED_BODY()
public:
    AGemCatalogPreview();
    bool ShowDefinition(const struct FTowerDefinition& Definition);
    void Rotate(float Degrees);
    void SetCapturing(bool bEnabled);
    UPROPERTY(Transient) TObjectPtr<class UTextureRenderTarget2D> Image;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Model;
private:
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Outline;
    UPROPERTY() TObjectPtr<class USceneCaptureComponent2D> Capture;
    float ModelYaw=0;
    void PositionModel();
};
