#include "GemCatalogPreview.h"
#include "GemPrototype.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"

AGemCatalogPreview::AGemCatalogPreview()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Model=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gem"));
    Model->SetupAttachment(RootComponent);
    Outline=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Outline"));
    Outline->SetupAttachment(Model);
    for(auto* Part:{Model.Get(),Outline.Get()})
    {
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(false);
        Part->SetVisibleInSceneCaptureOnly(true);
    }
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortraitCamera"));
    Capture->SetupAttachment(RootComponent);
    Capture->SetRelativeRotation(FRotator(-24,45,0));
    Capture->SetRelativeLocation(-Capture->GetRelativeRotation().Vector()*260.f);
    Capture->FOVAngle=35;
    Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame=false;
    Capture->bCaptureOnMovement=false;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetBloom(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
}

bool AGemCatalogPreview::ShowDefinition(const FTowerDefinition& Definition)
{
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Definition.Model);
    auto* Material=LoadObject<UMaterialInterface>(nullptr,*Definition.Material);
    Model->SetStaticMesh(Mesh); Model->EmptyOverrideMaterials(); Model->SetMaterial(0,Material);
    Outline->SetStaticMesh(Mesh);
    Outline->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_CelOutline.M_CelOutline")));
    ModelYaw=0; PositionModel();
    if(!Image)
    {
        Image=NewObject<UTextureRenderTarget2D>(this);
        Image->ClearColor=FLinearColor::Black;
        Image->RenderTargetFormat=RTF_RGBA8;
        Image->InitAutoFormat(1024,1024);
        Image->UpdateResourceImmediate(true);
        Capture->TextureTarget=Image;
        Capture->ShowOnlyComponent(Model); Capture->ShowOnlyComponent(Outline);
    }
    SetCapturing(true);
    return Mesh && Material;
}

void AGemCatalogPreview::PositionModel()
{
    if(!Model->GetStaticMesh()) return;
    const auto Bounds=Model->GetStaticMesh()->GetBoundingBox();
    const float Size=110.f/FMath::Max(1.f,Bounds.GetSize().GetMax());
    const FRotator Rotation(0,ModelYaw,0);
    Model->SetRelativeScale3D(FVector(Size)); Model->SetRelativeRotation(Rotation);
    Model->SetRelativeLocation(-Rotation.RotateVector(Bounds.GetCenter()*Size));
}
void AGemCatalogPreview::Rotate(float Degrees) { ModelYaw+=Degrees; PositionModel(); }
void AGemCatalogPreview::SetCapturing(bool bEnabled) { Capture->bCaptureEveryFrame=bEnabled; }
