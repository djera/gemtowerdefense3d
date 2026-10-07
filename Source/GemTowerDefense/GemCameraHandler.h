#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GemCameraHandler.generated.h"

/** Orbit and pan state for the board camera, independent of placement input. */
UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class UGemCameraHandler : public UActorComponent
{
    GENERATED_BODY()
public:
    static constexpr float DefaultYaw = 45.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") float Yaw = DefaultYaw;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") float Pitch = -35.264f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") FVector PanOffset = FVector::ZeroVector;
    UFUNCTION(BlueprintCallable, Category="Camera") void ResetView();
    UFUNCTION(BlueprintCallable, Category="Camera") void Orbit(FVector2D MouseDelta);
    UFUNCTION(BlueprintCallable, Category="Camera") void Pan(FVector2D MouseDelta, float UnitsPerPixel);
    void Apply(class UCameraComponent* Camera, int32 Width, int32 Height, float SidebarWidth) const;
};
