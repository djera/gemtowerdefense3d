#include "GemActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGemActor::AGemActor()
{
    PrimaryActorTick.bCanEverTick = false;
    GemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GemMesh"));
    SetRootComponent(GemMesh);
    const TCHAR* Types[] = {TEXT("Amethyst"),TEXT("Aquamarine"),TEXT("Diamond"),TEXT("Emerald"),TEXT("Opal"),TEXT("Ruby"),TEXT("Sapphire"),TEXT("Topaz")};
    const TCHAR* Qualities[] = {TEXT("Chipped"),TEXT("Flawed"),TEXT("Normal"),TEXT("Flawless"),TEXT("Perfect")};
    for (const TCHAR* T : Types)
        for (const TCHAR* Q : Qualities)
        {
            const FString Name = FString::Printf(TEXT("SM_%s_%s"), T, Q);
            const FString Path = FString::Printf(TEXT("/Game/Gems/Meshes/%s.%s"), *Name, *Name);
            ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*Path);
            Library.Add(Mesh.Object);
        }
}
void AGemActor::RefreshGem()
{
    const int32 Index = static_cast<int32>(Type) * 5 + static_cast<int32>(Quality);
    GemMesh->SetStaticMesh(Library.IsValidIndex(Index) ? Library[Index].Get() : nullptr);
}
void AGemActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshGem();
}
void AGemActor::SetGem(EGemType NewType, EGemQuality NewQuality)
{
    Type = NewType;
    Quality = NewQuality;
    RefreshGem();
}
