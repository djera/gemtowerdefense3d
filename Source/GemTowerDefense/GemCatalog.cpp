#include "GemPrototype.h"
#include "GemCatalogPreview.h"
#include "GemCameraHandler.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/StaticMeshComponent.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
struct FCatalogLayout
{
    float X=40,Y=90,W,H,ListX=60,ListY=202,ListW=208,PreviewX=286,PreviewY=202,PreviewSize,StatsX,StatsW,Bottom;
    int32 ListRows,StatsRows,Columns;
    FCatalogLayout(float Width,float Height):W(Width-80),H(Height-140)
    {
        Bottom=Y+H-48; PreviewSize=FMath::Clamp(W*.26f,240.f,320.f);
        StatsX=PreviewX+PreviewSize+24; StatsW=X+W-20-StatsX;
        ListRows=FMath::Max(1,FMath::FloorToInt((Bottom-ListY)/30));
        StatsRows=FMath::Max(1,FMath::FloorToInt((Bottom-ListY)/22));
        Columns=FMath::Max(16,FMath::FloorToInt((StatsW-18)/7.5f));
    }
};
bool Within(float X,float Y,float L,float T,float W,float H) { return X>=L && X<L+W && Y>=T && Y<T+H; }
FString Pretty(FString Text)
{
    Text.ReplaceInline(TEXT("_"),TEXT(" "));
    if(!Text.IsEmpty()) Text[0]=FChar::ToUpper(Text[0]);
    return Text;
}
FString NumberText(double Value)
{
    FString Text=FString::Printf(TEXT("%.3f"),Value);
    while(Text.EndsWith(TEXT("0"))) Text.LeftChopInline(1);
    Text.RemoveFromEnd(TEXT(".")); return Text;
}
// Keep the definition exhaustive: newly added stat/modifier fields appear automatically.
void JsonLines(TArray<FString>& Lines,const FString& Key,const TSharedPtr<FJsonValue>& Value)
{
    if(!Value) return;
    if(Value->Type==EJson::Object)
    {
        if(!Key.IsEmpty()) Lines.Add(Pretty(Key));
        TArray<FString> Keys; for(const auto& Pair:Value->AsObject()->Values) Keys.Add(FString(*Pair.Key)); Keys.Sort();
        for(const auto& Child:Keys) JsonLines(Lines,Child,Value->AsObject()->TryGetField(Child));
    }
    else if(Value->Type==EJson::Array)
    {
        if(!Key.IsEmpty()) Lines.Add(Pretty(Key));
        for(const auto& Item:Value->AsArray()) JsonLines(Lines,TEXT(""),Item);
    }
    else
    {
        FString Text;
        if(Value->Type==EJson::Number)
        {
            const double N=Value->AsNumber(); Text=NumberText(N);
            if(Key==TEXT("range") || Key==TEXT("radius")) Text+=TEXT(" cm / ")+NumberText(N/AGemBoard::CellSize)+TEXT(" tiles");
            if(Key==TEXT("chance") || Key==TEXT("fraction") || Key==TEXT("slow") || Key==TEXT("damage_per_level")) Text+=TEXT(" (")+NumberText(N*100)+TEXT("%)");
            if(Key==TEXT("duration") || Key==TEXT("attack_interval")) Text+=TEXT(" s");
            if(Key==TEXT("projectile_speed")) Text+=TEXT(" cm/s");
        }
        else if(Value->Type==EJson::Boolean) Text=Value->AsBool()?TEXT("Yes"):TEXT("No");
        else if(Value->Type==EJson::String) Text=Value->AsString();
        else Text=TEXT("None");
        Lines.Add(Key.IsEmpty()?Text:Pretty(Key)+TEXT(": ")+Text);
    }
}
TArray<FString> WrapStats(const TArray<FString>& Source,int32 Width)
{
    TArray<FString> Result;
    for(FString Line:Source)
    {
        while(Line.Len()>Width)
        {
            int32 Cut=Width;
            for(int32 I=Width;I>Width/2;--I) if(Line[I]==' ') { Cut=I; break; }
            Result.Add(Line.Left(Cut)); Line=Line.Mid(Cut).TrimStart();
        }
        Result.Add(Line);
    }
    return Result;
}
}

TArray<FString> AGemBoard::GemCatalogIds(bool bSpecial) const
{
    TArray<FString> Result;
    if(!bSpecial)
    {
        for(int32 T=0;T<GemTypes.Num();++T) for(int32 Q=0;Q<Qualities.Num();++Q)
            if(const auto* D=BaseDefinition(T,Q)) Result.Add(D->Id);
    }
    else
    {
        for(const auto& Pair:Definitions) if(!Pair.Value.bStandard && Pair.Key!=RockDefinition) Result.Add(Pair.Key);
        Result.Sort([&](const FString& A,const FString& B){return Definitions[A].Name<Definitions[B].Name;});
    }
    return Result;
}

TArray<FString> AGemBoard::GemCatalogStats(const FString& Id) const
{
    TArray<FString> Lines; const auto* D=FindDefinition(Id); if(!D) return Lines;
    Lines.Add(TEXT("ATTACK"));
    const float LevelMultiplier=1+D->InitialLevel*D->DamagePerLevel;
    Lines.Add(FString::Printf(TEXT("Damage: %s - %s"),*NumberText((D->Damage+D->Dice)*LevelMultiplier),*NumberText((D->Damage+D->Dice*D->Sides)*LevelMultiplier)));
    Lines.Add(FString::Printf(TEXT("Range: %s tiles (%s cm)"),*NumberText(D->Range/CellSize),*NumberText(D->Range)));
    Lines.Add(FString::Printf(TEXT("Attack interval: %s s"),*NumberText(D->Interval)));
    Lines.Add(FString(TEXT("Hits: "))+(D->bGround?(D->bAir?TEXT("Ground + air"):TEXT("Ground only")):(D->bAir?TEXT("Air only"):TEXT("None"))));
    Lines.Add(TEXT("")); Lines.Add(TEXT("MODIFIERS"));
    const TArray<TSharedPtr<FJsonValue>>* Modifiers=nullptr;
    if(D->Json->TryGetArrayField(TEXT("modifiers"),Modifiers) && !Modifiers->IsEmpty())
    {
        for(int32 I=0;I<Modifiers->Num();++I)
        {
            Lines.Add(FString::Printf(TEXT("%d. %s"),I+1,*Pretty((*Modifiers)[I]->AsObject()->GetStringField(TEXT("type")))));
            JsonLines(Lines,TEXT(""),(*Modifiers)[I]); Lines.Add(TEXT(""));
        }
    }
    else Lines.Add(TEXT("None"));
    Lines.Add(TEXT("")); Lines.Add(TEXT("ALL BASE STATS"));
    JsonLines(Lines,TEXT(""),D->Json->TryGetField(TEXT("stats")));
    Lines.Add(TEXT("")); Lines.Add(TEXT("UPGRADES & RECIPES"));
    if(D->bStandard)
    {
        const int32 QualityIndex=Qualities.IndexOfByPredicate([&](const auto& Q){return Q.Id==D->Quality;});
        const int32 TypeIndex=GemTypes.IndexOfByPredicate([&](const auto& T){return T.Id==D->BaseType;});
        TArray<int32> Counts; MergeRules.GetKeys(Counts); Counts.Sort(); bool bAny=false;
        for(int32 Count:Counts) if(const auto* Result=BaseDefinition(TypeIndex,QualityIndex+MergeRules[Count]))
        { Lines.Add(FString::Printf(TEXT("%d matching gems -> %s"),Count,*Result->Name)); bAny=true; }
        if(!bAny) Lines.Add(TEXT("Maximum merge grade"));
        bool bRollable=false;
        for(const auto& Level:ChanceLevels) bRollable|=Level.Weights.FindRef(D->Quality)>0;
        Lines.Add(bRollable?TEXT("Available through random placement"):TEXT("Combining only; not rolled on placement"));
    }
    if(const auto* Upgrade=FindDefinition(D->Upgrade)) Lines.Add(FString::Printf(TEXT("Upgrade: %s / %d gold"),*Upgrade->Name,D->UpgradeCost));
    for(const auto& Pair:Definitions) if(Pair.Value.Upgrade==Id) Lines.Add(TEXT("Upgraded from: ")+Pair.Value.Name);
    for(const auto& Recipe:Recipes) if(Recipe.Result==Id || Recipe.Ingredients.Contains(Id))
    {
        Lines.Add(Recipe.Result==Id?TEXT("Recipe to create this tower:"):TEXT("Ingredient in: ")+Recipe.Name);
        TArray<FString> Names;
        for(const auto& Ingredient:Recipe.Ingredients) { const auto* Part=FindDefinition(Ingredient); Names.Add(Part?Part->Name:Ingredient); }
        const auto* Result=FindDefinition(Recipe.Result);
        Lines.Add(FString::Join(Names,TEXT(" + "))+TEXT(" -> ")+(Result?Result->Name:Recipe.Result));
    }
    Lines.Add(TEXT("")); Lines.Add(TEXT("DAMAGE VS ARMOR (multipliers)"));
    const TSharedPtr<FJsonObject>* Table=nullptr;
    if(Catalog && Catalog->TryGetObjectField(TEXT("damage_table"),Table)) JsonLines(Lines,TEXT(""),(*Table)->TryGetField(D->BaseType));
    Lines.Add(TEXT("")); Lines.Add(TEXT("DEFINITION"));
    Lines.Add(TEXT("ID: ")+D->Id); Lines.Add(TEXT("Gem type: ")+D->BaseType); Lines.Add(TEXT("Quality: ")+D->Quality);
    Lines.Add(TEXT("Tower type: ")+D->TowerType); Lines.Add(TEXT("Model: ")+D->Model); Lines.Add(TEXT("Material: ")+D->Material);
    return Lines;
}

void AGemController::OpenGems()
{
    if(!Board) return;
    bGemsOpen=true; bRecipesOpen=false; bHover=false; bCameraDragging=false; bSpeedDragging=false; bHasPreviousMouse=false;
    Board->UpdateGhost(Hover,0,false);
    if(!CatalogPreview) CatalogPreview=GetWorld()->SpawnActor<AGemCatalogPreview>(FVector(100000,100000,10000),FRotator::ZeroRotator);
    const auto* SelectedDefinition=Board->Definition(Board->Selected);
    SetCatalogGroup(SelectedDefinition?!SelectedDefinition->bStandard:bCatalogSpecial);
    if(SelectedDefinition) SelectCatalogGem(CatalogIds.Find(SelectedDefinition->Id));
    // Keep the active entry visible even when opening directly from a selected gem.
    CatalogListScroll=FMath::Max(0,CatalogSelected-3);
}
void AGemController::CloseGems()
{
    bGemsOpen=false; bHasPreviousMouse=false;
    if(CatalogPreview) CatalogPreview->SetCapturing(false);
}
void AGemController::SetCatalogGroup(bool bSpecial)
{
    bCatalogSpecial=bSpecial; CatalogIds=Board->GemCatalogIds(bSpecial); CatalogListScroll=0;
    SelectCatalogGem(CatalogIds.IsEmpty()?INDEX_NONE:0);
}
void AGemController::SelectCatalogGem(int32 Index)
{
    CatalogSelected=CatalogIds.IsValidIndex(Index)?Index:INDEX_NONE; CatalogStatsScroll=0; CatalogStats.Reset();
    const auto* Definition=CatalogIds.IsValidIndex(CatalogSelected)?Board->FindDefinition(CatalogIds[CatalogSelected]):nullptr;
    if(Definition)
    {
        CatalogStats=Board->GemCatalogStats(Definition->Id);
        if(CatalogPreview) CatalogPreview->ShowDefinition(*Definition);
    }
    else if(CatalogPreview) CatalogPreview->SetCapturing(false);
}
bool AGemController::CatalogClick(float X,float Y,int32 W,int32 H)
{
    if(!bGemsOpen) return false;
    const FCatalogLayout L(W,H);
    if(Within(X,Y,L.X+L.W-128,L.Y+16,108,34)) CloseGems();
    else if(Within(X,Y,L.ListX,156,100,30)) SetCatalogGroup(false);
    else if(Within(X,Y,L.ListX+108,156,100,30)) SetCatalogGroup(true);
    else if(Within(X,Y,L.ListX,L.ListY,L.ListW,L.ListRows*30))
    {
        const int32 Index=CatalogListScroll+int32((Y-L.ListY)/30);
        if(CatalogIds.IsValidIndex(Index)) SelectCatalogGem(Index);
    }
    else if(CatalogPreview && Within(X,Y,L.PreviewX,L.PreviewY+L.PreviewSize+12,L.PreviewSize,32))
        CatalogPreview->Rotate(X<L.PreviewX+L.PreviewSize/2?-30:30);
    return true; // Modal: consume clicks even outside the window.
}
void AGemController::HandleCatalogInput(float X,float Y,bool bHasMouse,int32 W,int32 H)
{
    if(WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::G)) { CloseGems(); return; }
    const FCatalogLayout L(W,H);
    if(bHasMouse && WasInputKeyJustPressed(EKeys::LeftMouseButton)) CatalogClick(X,Y,W,H);
    const int32 Wheel=(WasInputKeyJustPressed(EKeys::MouseScrollDown)?1:0)-(WasInputKeyJustPressed(EKeys::MouseScrollUp)?1:0);
    if(bHasMouse && Wheel)
    {
        if(Within(X,Y,L.ListX,L.ListY,L.ListW,L.Bottom-L.ListY)) CatalogListScroll=FMath::Clamp(CatalogListScroll+Wheel*3,0,FMath::Max(0,CatalogIds.Num()-L.ListRows));
        else if(Within(X,Y,L.StatsX,L.ListY,L.StatsW,L.Bottom-L.ListY)) CatalogStatsScroll=FMath::Clamp(CatalogStatsScroll+Wheel*3,0,FMath::Max(0,WrapStats(CatalogStats,L.Columns).Num()-L.StatsRows));
    }
    const int32 Step=(WasInputKeyJustPressed(EKeys::Down)?1:0)-(WasInputKeyJustPressed(EKeys::Up)?1:0);
    if(Step && !CatalogIds.IsEmpty())
    {
        SelectCatalogGem(FMath::Clamp(CatalogSelected+Step,0,CatalogIds.Num()-1));
        CatalogListScroll=FMath::Clamp(CatalogListScroll,FMath::Max(0,CatalogSelected-L.ListRows+1),CatalogSelected);
    }
    if(CatalogPreview && WasInputKeyJustPressed(EKeys::Left)) CatalogPreview->Rotate(-30);
    if(CatalogPreview && WasInputKeyJustPressed(EKeys::Right)) CatalogPreview->Rotate(30);
}

void AGemHUD::DrawGemCatalog(AGemController* PC)
{
    const float Scale=AGemController::UIScale(Canvas->SizeX,Canvas->SizeY),W=Canvas->SizeX/Scale,H=Canvas->SizeY/Scale;
    const FCatalogLayout L(W,H);
    const FLinearColor Ink(.8f,.86f,.93f),Muted(.46f,.56f,.65f),Accent(.27f,.85f,.73f),Panel(.035f,.055f,.078f);
    auto Rect=[&](FLinearColor Color,float X,float Y,float Width,float Height){DrawRect(Color,X*Scale,Y*Scale,Width*Scale,Height*Scale);};
    auto Label=[&](const FString& Text,float X,float Y,FLinearColor Color,float Size=.8f){DrawText(Text,Color,X*Scale,Y*Scale,GEngine->GetMediumFont(),Size*Scale);};
    auto FitLabel=[&](const FString& Text,float X,float Y,float Width,FLinearColor Color,float Size=.8f)
    {
        float TextWidth,TextHeight; GetTextSize(Text,TextWidth,TextHeight,GEngine->GetMediumFont(),Size);
        Label(Text,X,Y,Color,Size*FMath::Min(1.f,Width/FMath::Max(1.f,TextWidth)));
    };
    Rect(FLinearColor(0,0,0,.68f),0,0,W,H);
    Rect(FLinearColor(.12f,.27f,.30f),L.X-1,L.Y-1,L.W+2,L.H+2);
    Rect(FLinearColor(.019f,.031f,.047f),L.X,L.Y,L.W,L.H);
    Label(TEXT("GEMS"),L.X+20,L.Y+20,Accent,1.35f);
    Label(TEXT("Browse grades, special towers and abilities"),L.X+122,L.Y+25,Ink,.86f);
    Rect(Panel,L.X+L.W-128,L.Y+16,108,34); Label(TEXT("CLOSE / ESC"),L.X+L.W-118,L.Y+26,Accent,.72f);
    Rect(!PC->bCatalogSpecial?FLinearColor(.08f,.25f,.25f):Panel,L.ListX,156,100,30);
    Rect(PC->bCatalogSpecial?FLinearColor(.08f,.25f,.25f):Panel,L.ListX+108,156,100,30);
    Label(TEXT("BASE GEMS"),L.ListX+10,165,Accent,.72f); Label(TEXT("SPECIAL"),L.ListX+119,165,Accent,.72f);
    const auto* D=PC->CatalogIds.IsValidIndex(PC->CatalogSelected)?PC->Board->FindDefinition(PC->CatalogIds[PC->CatalogSelected]):nullptr;
    if(D) FitLabel(D->Name,L.PreviewX,162,L.PreviewSize,Accent,1.05f);
    Label(TEXT("STATS & MODIFIERS"),L.StatsX,162,Accent,.9f);
    Rect(Panel,L.ListX,L.ListY,L.ListW,L.Bottom-L.ListY);
    PC->CatalogListScroll=FMath::Clamp(PC->CatalogListScroll,0,FMath::Max(0,PC->CatalogIds.Num()-L.ListRows));
    for(int32 Row=0;Row<L.ListRows;++Row)
    {
        const int32 Index=PC->CatalogListScroll+Row; if(!PC->CatalogIds.IsValidIndex(Index)) break;
        const auto* Entry=PC->Board->FindDefinition(PC->CatalogIds[Index]); if(!Entry) continue;
        const float Y=L.ListY+Row*30;
        if(Index==PC->CatalogSelected) { Rect(FLinearColor(.08f,.25f,.25f),L.ListX,Y,L.ListW-5,29); Rect(Accent,L.ListX,Y,3,29); }
        FitLabel(Entry->Name,L.ListX+12,Y+8,L.ListW-28,Index==PC->CatalogSelected?Accent:Ink,.77f);
    }
    auto Scrollbar=[&](float X,float Y,float Height,int32 Total,int32 Visible,int32 Offset)
    {
        if(Total<=Visible) return;
        Rect(FLinearColor(.1f,.15f,.19f),X,Y,3,Height);
        const float Thumb=Height*Visible/Total;
        Rect(Accent,X,Y+(Height-Thumb)*Offset/(Total-Visible),3,Thumb);
    };
    Scrollbar(L.ListX+L.ListW-3,L.ListY,L.ListRows*30,PC->CatalogIds.Num(),L.ListRows,PC->CatalogListScroll);
    Rect(FLinearColor::Black,L.PreviewX,L.PreviewY,L.PreviewSize,L.PreviewSize);
    if(D && PC->CatalogPreview && PC->CatalogPreview->Model->GetStaticMesh() && PC->CatalogPreview->Image)
        DrawTexture(PC->CatalogPreview->Image,L.PreviewX*Scale,L.PreviewY*Scale,L.PreviewSize*Scale,L.PreviewSize*Scale,0,0,1,1,FLinearColor::White,BLEND_Opaque);
    else Label(TEXT("No model available"),L.PreviewX+22,L.PreviewY+60,Muted);
    Rect(Panel,L.PreviewX,L.PreviewY+L.PreviewSize+12,L.PreviewSize,32);
    Label(TEXT("< ROTATE"),L.PreviewX+16,L.PreviewY+L.PreviewSize+22,Accent,.74f);
    Label(TEXT("ROTATE >"),L.PreviewX+L.PreviewSize-91,L.PreviewY+L.PreviewSize+22,Accent,.74f);
    if(D)
    {
        Label(D->bStandard?D->BaseType+TEXT(" / ")+D->Quality:TEXT("Special tower / ")+D->BaseType,L.PreviewX,L.PreviewY+L.PreviewSize+62,Ink,.78f);
        Label(TEXT("Base values; no live buffs or kills."),L.PreviewX,L.PreviewY+L.PreviewSize+86,Muted,.7f);
        Label(TEXT("Preview uses the assigned game model."),L.PreviewX,L.PreviewY+L.PreviewSize+106,Muted,.68f);
    }
    auto Lines=WrapStats(PC->CatalogStats,L.Columns);
    PC->CatalogStatsScroll=FMath::Clamp(PC->CatalogStatsScroll,0,FMath::Max(0,Lines.Num()-L.StatsRows));
    Rect(Panel,L.StatsX-10,L.ListY-4,L.StatsW+10,L.Bottom-L.ListY+4);
    for(int32 Row=0;Row<L.StatsRows && PC->CatalogStatsScroll+Row<Lines.Num();++Row)
    {
        const FString& Line=Lines[PC->CatalogStatsScroll+Row];
        const bool Heading=!Line.IsEmpty() && Line.Equals(Line.ToUpper(),ESearchCase::CaseSensitive);
        Label(Line,L.StatsX,L.ListY+Row*22,Heading?Accent:Ink,.8f);
    }
    Scrollbar(L.StatsX+L.StatsW-3,L.ListY,L.StatsRows*22,Lines.Num(),L.StatsRows,PC->CatalogStatsScroll);
    Label(FString::Printf(TEXT("%d gems / scroll or Up & Down"),PC->CatalogIds.Num()),L.ListX,L.Bottom+14,Muted,.7f);
    Label(TEXT("Left / Right: rotate preview"),L.PreviewX,L.Bottom+14,Muted,.7f);
    Label(TEXT("Scroll for all stats / waves continue while browsing"),L.StatsX,L.Bottom+14,Muted,.7f);
}

void AGemController::RunCatalogChecks()
{
    int32 Passed=0,Failed=0;
    auto Check=[&](bool Valid,const TCHAR* Name){ if(Valid) ++Passed; else ++Failed; UE_LOG(LogTemp,Display,TEXT("GEM_CATALOG_%s: %s"),Valid?TEXT("PASS"):TEXT("FAIL"),Name); };
    const int32 Before=Board->GemCount(); const int64 BeforeGold=Gold;
    OpenGems();
    Check(CatalogIds.Num()==Board->GemTypes.Num()*Board->Qualities.Num(),TEXT("All base gem grades are browsable"));
    bool bModels=true,bStats=true;
    for(bool bSpecial:{false,true})
    {
        SetCatalogGroup(bSpecial);
        for(int32 I=0;I<CatalogIds.Num();++I)
        {
            SelectCatalogGem(I); const auto* D=Board->FindDefinition(CatalogIds[I]);
            bModels&=CatalogPreview && CatalogPreview->Model->GetStaticMesh() && CatalogPreview->Model->GetMaterial(0);
            bStats&=D && CatalogStats.Contains(TEXT("ID: ")+D->Id) && CatalogStats.Contains(TEXT("MODIFIERS")) && CatalogStats.Contains(TEXT("ALL BASE STATS"));
        }
    }
    Check(bModels && CatalogIds.Num()>0,TEXT("Every base and special definition loads its actual model and material"));
    Check(bStats,TEXT("Every catalog entry exposes its stats and modifiers"));
    const FCatalogLayout L(1440,900);
    CatalogClick(L.ListX+10,165,1440,900); CatalogClick(L.ListX+10,L.ListY+4*30+10,1440,900);
    Check(!bCatalogSpecial && CatalogSelected==4,TEXT("Tabs and list clicks select the expected catalog entry"));
    CatalogClick(10,300,1440,900);
    Check(Board->GemCount()==Before && Gold==BeforeGold && bGemsOpen,TEXT("Browsing and outside clicks do not place gems or spend gold"));
    CatalogClick(L.X+L.W-80,L.Y+30,1440,900);
    Check(!bGemsOpen,TEXT("Close button dismisses the modal"));
    CameraHandler->Orbit(FVector2D(120,30)); ResetCamera();
    Check(FMath::IsNearlyEqual(CameraHandler->Yaw,45.f),TEXT("Reset camera uses the clockwise-rotated start angle"));
    UE_LOG(LogTemp,Display,TEXT("GEM_CATALOG_RESULTS: %d passed / %d failed"),Passed,Failed);
    OpenGems(); SelectCatalogGem(CatalogIds.Find(TEXT("Sapphire_Perfect"))); CatalogListScroll=CatalogSelected-3;
    FTimerHandle CaptureTimer,SpecialTimer,SpecialCapture,CloseTimer,BoardCapture,ExitTimer;
    GetWorldTimerManager().SetTimer(CaptureTimer,FTimerDelegate::CreateLambda([this](){FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("GemCatalogPreview.png"),true,false);}),8.f,false);
    GetWorldTimerManager().SetTimer(SpecialTimer,FTimerDelegate::CreateLambda([this](){SetCatalogGroup(true); SelectCatalogGem(CatalogIds.Find(TEXT("Paraiba"))); CatalogListScroll=CatalogSelected-3;}),10.f,false);
    GetWorldTimerManager().SetTimer(SpecialCapture,FTimerDelegate::CreateLambda([this](){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GemSpecialCatalogPreview.png"),true,false);}),12.f,false);
    GetWorldTimerManager().SetTimer(CloseTimer,FTimerDelegate::CreateLambda([this](){CloseGems(); Board->Place(FIntPoint(10,14),6,4); Board->SelectAt(FIntPoint(10,14));}),14.f,false);
    GetWorldTimerManager().SetTimer(BoardCapture,FTimerDelegate::CreateLambda([this](){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GemRangePreview.png"),true,false);}),16.f,false);
    GetWorldTimerManager().SetTimer(ExitTimer,FTimerDelegate::CreateLambda([this](){ConsoleCommand(TEXT("quit"));}),18.f,false);
}
