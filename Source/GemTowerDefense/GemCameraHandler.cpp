#include "GemCameraHandler.h"
#include "Camera/CameraComponent.h"

void UGemCameraHandler::ResetView()
{
    Yaw=DefaultYaw; Pitch=-35.264f; PanOffset=FVector::ZeroVector;
}
void UGemCameraHandler::Orbit(FVector2D MouseDelta)
{
    Yaw=FRotator::NormalizeAxis(Yaw+MouseDelta.X*.25f);
    Pitch=FMath::Clamp(Pitch+MouseDelta.Y*.20f,-80.f,-20.f);
}
void UGemCameraHandler::Pan(FVector2D MouseDelta,float UnitsPerPixel)
{
    const FRotator Rotation(Pitch,Yaw,0);
    const FVector Right=FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
    const FVector Forward=FRotator(0,Yaw,0).Vector();
    const float VerticalScale=FMath::Max(.1f,FMath::Sin(FMath::DegreesToRadians(-Pitch)));
    PanOffset-=Right*MouseDelta.X*UnitsPerPixel;
    PanOffset+=Forward*MouseDelta.Y*UnitsPerPixel/VerticalScale;
    PanOffset.X=FMath::Clamp(PanOffset.X,-3000.f,3000.f);
    PanOffset.Y=FMath::Clamp(PanOffset.Y,-3000.f,3000.f);
    PanOffset.Z=0;
}
void UGemCameraHandler::Apply(UCameraComponent* Camera,int32 Width,int32 Height,float SidebarWidth) const
{
    if (!Camera || Width<=0 || Height<=0) return;
    Camera->SetWorldRotation(FRotator(Pitch,Yaw,0));
    const FVector Right=Camera->GetRightVector(),Up=Camera->GetUpVector();
    const float Aspect=float(Width)/Height,Header=64.f,Footer=100.f;
    const float BoardWidth=2000.f*(FMath::Abs(Right.X)+FMath::Abs(Right.Y));
    const float BoardHeight=2000.f*(FMath::Abs(Up.X)+FMath::Abs(Up.Y))+180.f;
    const float Ortho=FMath::Max(BoardWidth/FMath::Max(.2f,1-SidebarWidth/Width),
        BoardHeight*Aspect/FMath::Max(.2f,1-(Header+Footer)/Height))*1.1f;
    Camera->SetOrthoWidth(Ortho);
    Camera->SetWorldLocation(PanOffset-Camera->GetForwardVector()*3500.f
        +Right*(Ortho*SidebarWidth/Width*.5f)+Up*(Ortho/Aspect*(Header-Footer)/Height*.5f));
}
