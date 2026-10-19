#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
// Opt-in, disposable rendered functional test. Does not save maps/settings or change gameplay code.
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/CanvasPanel.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "UObject/UnrealType.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/Controller/SFRTSCameraPawn.h"
#include "InGame/HUD/Minimap/SFMinimapPanelWidget.h"
#include "InGame/SFInGameHUDWidget.h"
#include "UnrealClient.h"

namespace SFMinimapRenderedQA
{
struct FStep { float Delay; TFunction<void()> Action; };
struct FRun : TSharedFromThis<FRun>
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ASFRTSCameraPawn> Camera;
    TWeakObjectPtr<USFMinimapPanelWidget> Minimap;
    TWeakObjectPtr<UWidget> Layer;
    TWeakObjectPtr<AActor> Wall;
    TArray<FStep> Steps;
    int32 Step = 0, Repeat = 0, Case = 0;
    float Elapsed = 0;
    bool bHeld = false, bOriginalBounds = false, bOriginalEdge = true;
    FVector2D Minimum, Maximum, OriginalMinimum, OriginalMaximum;
    FVector Initial, OriginalLocation, Start, Target, Expected, Before;
    FIntPoint OriginalSize;
    FString Folder, Rows = TEXT("repeat,case,check,passed,actual_x,actual_y,expected_x,expected_y,detail\n");
    FVector2D Cursor = FVector2D::ZeroVector;
    int32 Failed = 0, Assertions = 0;

    template<typename T> T Read(const TCHAR* Name) const
    {
        const FProperty* P = Camera->GetClass()->FindPropertyByName(Name);
        return P ? *P->ContainerPtrToValuePtr<T>(Camera.Get()) : T{};
    }
    bool Bounds() const
    { auto* P=FindFProperty<FBoolProperty>(Camera->GetClass(),TEXT("bConstrainCameraToBounds")); return P && P->GetPropertyValue_InContainer(Camera.Get()); }
    bool Edge() const
    { auto* P=FindFProperty<FBoolProperty>(Camera->GetClass(),TEXT("bEnableEdgeScroll")); return P && P->GetPropertyValue_InContainer(Camera.Get()); }
    void SetBounds(bool Value)
    { FindFProperty<FBoolProperty>(Camera->GetClass(),TEXT("bConstrainCameraToBounds"))->SetPropertyValue_InContainer(Camera.Get(),Value); }
    UFloatingPawnMovement* Movement() const { return Camera->FindComponentByClass<UFloatingPawnMovement>(); }
    bool Captured() const { return Minimap->TakeWidget()->HasMouseCapture(); }
    FVector Clamp(FVector Value) const
    { Value.X=FMath::Clamp(Value.X,Minimum.X,Maximum.X); Value.Y=FMath::Clamp(Value.Y,Minimum.Y,Maximum.Y); Value.Z=Initial.Z; return Value; }
    void Check(bool Pass,const FString& Name,const FString& Detail=TEXT(""))
    {
        ++Assertions; if(!Pass) ++Failed;
        const FVector Actual=Camera->GetActorLocation();
        FString Safe=Detail; Safe.ReplaceInline(TEXT(","),TEXT(";")); Safe.ReplaceInline(TEXT("\n"),TEXT(" "));
        Rows+=FString::Printf(TEXT("%d,TC%02d,%s,%d,%.3f,%.3f,%.3f,%.3f,%s\n"),Repeat,Case,*Name,Pass,Actual.X,Actual.Y,Expected.X,Expected.Y,*Safe);
        FFileHelper::SaveStringToFile(Rows,*(Folder/TEXT("assertions.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp,Display,TEXT("MINIMAP_RENDERED_QA r%d TC%02d %s %s %s"),Repeat,Case,Pass?TEXT("PASS"):TEXT("FAIL"),*Name,*Detail);
    }
    void SaveShot(const FString& Suffix=TEXT(""))
    { FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("r%d_TC%02d%s.png"),Repeat,Case,*Suffix),true,false); }
    FVector2D Screen(FVector Location) const
    { return Layer->GetCachedGeometry().LocalToAbsolute(Minimap->WorldToMinimapPosition(Location)); }
    FVector2D ViewportPoint(float X,float Y) const
    {
        const auto G=World->GetGameViewport()->GetGameViewportWidget()->GetCachedGeometry();
        return G.LocalToAbsolute(G.GetLocalSize()*FVector2D(X,Y));
    }
    void Move(FVector2D Position)
    {
        const FVector2D Previous=Cursor; Cursor=Position;
        FSlateApplication::Get().SetCursorPos(Position);
        TSet<FKey> Buttons; if(bHeld) Buttons.Add(EKeys::LeftMouseButton);
        FPointerEvent E(0,Position,Previous,Buttons,EKeys::Invalid,0,FModifierKeysState());
        FSlateApplication::Get().ProcessMouseMoveEvent(E);
    }
    void Down(FVector2D Position)
    {
        Move(Position); bHeld=true;
        FPointerEvent E(0,Position,Position,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
        const auto Window=World->GetGameViewport()->GetWindow();
        FSlateApplication::Get().ProcessMouseButtonDownEvent(Window.IsValid()?Window->GetNativeWindow():nullptr,E);
    }
    void Up()
    {
        bHeld=false;
        FPointerEvent E(0,Cursor,Cursor,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState());
        FSlateApplication::Get().ProcessMouseButtonUpEvent(E);
    }
    void Reset(int32 Id)
    {
        Case=Id; if(bHeld) Up(); FSlateApplication::Get().ReleaseAllPointerCapture();
        if(Wall.IsValid()) Wall->Destroy();
        SetBounds(true); Camera->SetEdgeScrollEnabled(true);
        Camera->FocusOnWorldLocation(Initial); Movement()->StopMovementImmediately();
        Move(ViewportPoint(.5f,.35f)); Expected=Camera->GetActorLocation();
    }
    void MakeWall()
    {
        AActor* A=World->SpawnActor<AActor>(); Wall=A;
        UBoxComponent* Box=NewObject<UBoxComponent>(A); A->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(25,500,1000)); Box->SetCollisionObjectType(ECC_GameTraceChannel2);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
        A->SetActorLocation((Start+Target)*.5f);
    }
    void Add(float Delay,TFunction<void()> Action) { Steps.Add({Delay,MoveTemp(Action)}); }
    void Queue(int32 R)
    {
        Add(.3f,[this,R]{Repeat=R;Reset(1); const FVector Mid((Minimum.X+Maximum.X)*.5,(Minimum.Y+Maximum.Y)*.5,Initial.Z);
            const float Distance=FMath::Min(1000.,(Maximum.X-Minimum.X)*.2); Start=Mid-FVector(Distance,0,0);Target=Mid+FVector(Distance,0,0); Expected=Clamp(Target);
            Camera->SetActorLocation(Start,false);MakeWall();FHitResult Hit;Camera->SetActorLocation(Target,true,&Hit);
            Check(Hit.bBlockingHit && !Camera->GetActorLocation().Equals(Expected,2.),TEXT("legacy_sweep_negative_control"),TEXT("current fixture replays old sweep operation; not an old executable"));
            Camera->SetActorLocation(Start,false);Down(Screen(Target));Check(Captured(),TEXT("real_slate_drag_capture"));Check(Camera->GetActorLocation().Equals(Expected,2.),TEXT("focus_crosses_wall"));Up();Move(ViewportPoint(.5f,.35f));});
        Add(.15f,[this]{Check(Camera->GetActorLocation().Equals(Expected,3.),TEXT("wall_jump_stable_after_click"));SaveShot();});

        for(int32 Corner=0;Corner<4;++Corner)
        {
            Add(.2f,[this,Corner]{if(Corner==0)Reset(2); Target=FVector(Corner&1?Maximum.X:Minimum.X,Corner&2?Maximum.Y:Minimum.Y,Initial.Z);Expected=Clamp(Target);Down(Screen(Target));Check(Captured(),FString::Printf(TEXT("corner_%d_capture"),Corner));Check(Camera->GetActorLocation().Equals(Expected,3.),FString::Printf(TEXT("corner_%d_clamp"),Corner));Up();Move(ViewportPoint(.5f,.35f));});
            Add(.12f,[this,Corner]{Check(Camera->GetActorLocation().Equals(Expected,3.),FString::Printf(TEXT("corner_%d_stable_after_release"),Corner));if(Corner==3)SaveShot();});
        }

        Add(.25f,[this]{Reset(3);Target=Clamp(Initial+FVector(350,200,0));Expected=Target; Movement()->Velocity=FVector(500,300,0);Camera->AddMovementInput(FVector::ForwardVector,1,true);Down(Screen(Target));
            Check(Movement()->Velocity.IsNearlyZero(),TEXT("click_clears_velocity"));Check(Camera->GetPendingMovementInputVector().IsNearlyZero(),TEXT("click_clears_pending_input"));Check(Camera->GetActorLocation().Equals(Expected,3.),TEXT("moving_camera_reaches_click"));Up();Move(ViewportPoint(.5f,.35f));Before=Camera->GetActorLocation();});
        Add(.35f,[this]{Check(Camera->GetActorLocation().Equals(Before,3.),TEXT("no_post_click_drift"));SaveShot();});

        Add(.2f,[this]{Reset(4);Movement()->Velocity=FVector(200,150,0);Move(Screen(Initial));Check(Movement()->Velocity.Equals(FVector(200,150,0)),TEXT("hover_preserves_velocity"));Movement()->StopMovementImmediately();Before=Camera->GetActorLocation();Move(ViewportPoint(.999f,.8f));});
        Add(.5f,[this]{Check(FVector::Dist2D(Before,Camera->GetActorLocation())>20.,TEXT("hover_allows_edge_scroll"));SaveShot();Move(ViewportPoint(.5f,.35f));});

        Add(.2f,[this]{Reset(5);Down(Screen(Initial));Check(Captured(),TEXT("drag_capture_started"));Move(ViewportPoint(.5f,.35f));});
        Add(.15f,[this]{Check(Captured(),TEXT("capture_retained_outside_minimap"));const FVector P=Camera->GetActorLocation();Check(P.Equals(Clamp(P),2.),TEXT("outside_drag_clamped"));SaveShot(TEXT("_held"));Up();Check(!Captured(),TEXT("outside_mouseup_releases_capture"));Move(ViewportPoint(.5f,.35f));Before=Camera->GetActorLocation();});
        Add(.25f,[this]{Check(Camera->GetActorLocation().Equals(Before,3.),TEXT("move_after_mouseup_does_not_drag"));Camera->FocusOnWorldLocation(Initial);Before=Camera->GetActorLocation();Move(ViewportPoint(.001f,.3f));});
        Add(.45f,[this]{Check(FVector::Dist2D(Before,Camera->GetActorLocation())>20.,TEXT("edge_scroll_restored_after_mouseup"));SaveShot();Move(ViewportPoint(.5f,.35f));});

        Add(.2f,[this]{Reset(6);Down(Screen(Initial));Check(Captured(),TEXT("capture_present_before_loss"));Move(ViewportPoint(.5f,.35f));FSlateApplication::Get().ReleaseAllPointerCapture();Check(!Captured(),TEXT("capture_lost_callback_path"));Up();Camera->FocusOnWorldLocation(Initial);Before=Camera->GetActorLocation();Move(ViewportPoint(.001f,.3f));});
        Add(.45f,[this]{Check(FVector::Dist2D(Before,Camera->GetActorLocation())>20.,TEXT("edge_scroll_restored_after_capture_loss"));SaveShot();Move(ViewportPoint(.5f,.35f));});

        Add(.2f,[this]{Reset(7);Camera->SetEdgeScrollEnabled(false);Move(Screen(Initial));Down(Screen(Initial));Up();Check(!Edge(),TEXT("disabled_preference_survives_hover_drag_release"));Before=Camera->GetActorLocation();Move(ViewportPoint(.001f,.3f));});
        Add(.45f,[this]{Check(Camera->GetActorLocation().Equals(Before,3.),TEXT("disabled_edge_scroll_stays_still"));Camera->SetEdgeScrollEnabled(true);Before=Camera->GetActorLocation();});
        Add(.45f,[this]{Check(Edge() && FVector::Dist2D(Before,Camera->GetActorLocation())>20.,TEXT("enabled_edge_scroll_moves"));SaveShot();Move(ViewportPoint(.5f,.35f));});

        Add(.2f,[this]{Reset(8);const FVector Mid((Minimum.X+Maximum.X)*.5,(Minimum.Y+Maximum.Y)*.5,Initial.Z); const float D=FMath::Min(1000.,(Maximum.X-Minimum.X)*.2); Start=Mid-FVector(D,0,0);Target=Mid+FVector(D,0,0);Expected=Target;
            SetBounds(false);Camera->SetActorLocation(Start,false);MakeWall();Down(Screen(Target));Check(!Bounds() && Camera->GetActorLocation().X>Start.X && Camera->GetActorLocation().X<((Start.X+Target.X)*.5) && FMath::Abs(Camera->GetActorLocation().Y-Start.Y)<2.,TEXT("unconfigured_bounds_preserve_collision"),TEXT("same map with runtime bounds disabled; controlled fixture, not a second authored map"));Up();Move(ViewportPoint(.5f,.35f));});
        Add(.15f,[this]{Wall->Destroy();SetBounds(true);Camera->FocusOnWorldLocation(Initial);});
        for(const FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080)})
        {
            Add(.2f,[this,Size]{World->GetGameViewport()->ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%dw"),Size.X,Size.Y));});
            Add(1.5f,[this,Size]{const FIntPoint Actual=World->GetGameViewport()->Viewport->GetSizeXY();Check(Actual==Size,TEXT("resolution_applied"),FString::Printf(TEXT("actual=%dx%d expected=%dx%d"),Actual.X,Actual.Y,Size.X,Size.Y));Target=Clamp(Initial+FVector(300,-200,0));Expected=Target;Down(Screen(Target));Check(Captured() && Camera->GetActorLocation().Equals(Expected,3.),TEXT("resized_minimap_click_alignment"),FString::Printf(TEXT("%dx%d"),Size.X,Size.Y));Up();Move(ViewportPoint(.5f,.35f));});
            Add(.15f,[this,Size]{Check(Camera->GetActorLocation().Equals(Expected,3.),TEXT("resized_click_stable_after_release"));SaveShot(FString::Printf(TEXT("_%dx%d"),Size.X,Size.Y));});
        }
    }
    bool Init()
    {
        UWorld* W=World.Get(); if(!W || !W->IsGameWorld() || !W->GetGameViewport())return false;
        auto* PC=W->GetFirstPlayerController<ASFInGamePlayerController>();if(!PC)return false;
        Camera=Cast<ASFRTSCameraPawn>(PC->GetPawn());if(!Camera.IsValid())return false;
        for(TObjectIterator<USFMinimapPanelWidget> It;It;++It)if(It->GetWorld()==W && It->IsVisible() && It->GetCachedGeometry().GetLocalSize().X>10){Minimap=*It;break;}
        if(!Minimap.IsValid())return false;
        auto* P=FindFProperty<FObjectPropertyBase>(Minimap->GetClass(),TEXT("MinimapMarkerLayer"));Layer=P?Cast<UWidget>(P->GetObjectPropertyValue_InContainer(Minimap.Get())):nullptr;
        if(!Layer.IsValid()||Layer->GetCachedGeometry().GetLocalSize().X<10)return false;
        bOriginalBounds=Bounds();bOriginalEdge=Edge();OriginalMinimum=Read<FVector2D>(TEXT("MinimumCameraBounds"));OriginalMaximum=Read<FVector2D>(TEXT("MaximumCameraBounds"));OriginalLocation=Camera->GetActorLocation();
        const auto* MinProp=Minimap->GetClass()->FindPropertyByName(TEXT("PlayableWorldMin"));const auto* MaxProp=Minimap->GetClass()->FindPropertyByName(TEXT("PlayableWorldMax"));
        if(!MinProp||!MaxProp)return false;
        const FVector2D MapMin=*MinProp->ContainerPtrToValuePtr<FVector2D>(Minimap.Get());const FVector2D MapMax=*MaxProp->ContainerPtrToValuePtr<FVector2D>(Minimap.Get());
        Camera->SetStageCameraBounds(FBox(FVector(MapMin,OriginalLocation.Z-1000),FVector(MapMax,OriginalLocation.Z+1000)),50);
        Minimum=Read<FVector2D>(TEXT("MinimumCameraBounds"));Maximum=Read<FVector2D>(TEXT("MaximumCameraBounds"));Initial=OriginalLocation;
        Initial=FVector((Minimum.X+Maximum.X)*.5,(Minimum.Y+Maximum.Y)*.5,Initial.Z);OriginalSize=W->GetGameViewport()->Viewport->GetSizeXY();
        FString Metadata=FString::Printf(TEXT("map=%s\nengine=%s\nviewport=%dx%d\noriginal_bounds=%d\nminimum=%s\nmaximum=%s\nminimap=%s\ninput=Slate routed pointer events (not NativeOnMouse overrides)\n"),*W->GetMapName(),*FEngineVersion::Current().ToString(),OriginalSize.X,OriginalSize.Y,bOriginalBounds,*Minimum.ToString(),*Maximum.ToString(),*Minimap->GetClass()->GetPathName());
        FFileHelper::SaveStringToFile(Metadata,*(Folder/TEXT("environment.txt")));
        for(int32 R=1;R<=3;++R)Queue(R);
        Add(.5f,[this]{if(bHeld)Up();*Camera->GetClass()->FindPropertyByName(TEXT("MinimumCameraBounds"))->ContainerPtrToValuePtr<FVector2D>(Camera.Get())=OriginalMinimum;*Camera->GetClass()->FindPropertyByName(TEXT("MaximumCameraBounds"))->ContainerPtrToValuePtr<FVector2D>(Camera.Get())=OriginalMaximum;SetBounds(bOriginalBounds);Camera->SetEdgeScrollEnabled(bOriginalEdge);Camera->SetActorLocation(OriginalLocation,false);if(Wall.IsValid())Wall->Destroy();World->GetGameViewport()->ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%dw"),OriginalSize.X,OriginalSize.Y));
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("assertions=%d\nfailed_assertions=%d\nrepetitions=3\nscenario_executions=24\n"),Assertions,Failed),*(Folder/TEXT("summary.txt")));UE_LOG(LogTemp,Display,TEXT("MINIMAP_RENDERED_QA COMPLETE assertions=%d failed=%d"),Assertions,Failed);});
        return true;
    }
    bool Tick(float Delta)
    {
        if(!World.IsValid())return false;Elapsed+=Delta;
        if(Steps.IsEmpty()) {if(Elapsed<8)return true;if(!Init()){if(Elapsed<60)return true;FFileHelper::SaveStringToFile(TEXT("BLOCKED: runtime fixture could not resolve live camera/minimap within 60 seconds"),*(Folder/TEXT("blocked.txt")));return false;}Elapsed=0;}
        if(Step>=Steps.Num())return false;
        if(Elapsed>=Steps[Step].Delay){Elapsed=0;Steps[Step++].Action();}
        return true;
    }
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommandWithWorldAndArgs Probe(TEXT("SF.Minimap.Probe"),TEXT("Read-only snapshot after a native mouse click; optional name."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* W)
    {
        if(!W||!W->IsGameWorld())return;
        auto* PC=W->GetFirstPlayerController<ASFInGamePlayerController>();auto* Cam=PC?Cast<ASFRTSCameraPawn>(PC->GetPawn()):nullptr;
        if(!Cam)return;
        FString Text=FString::Printf(TEXT("camera=%s\nviewport=%s\n"),*Cam->GetActorLocation().ToString(),*W->GetGameViewport()->Viewport->GetSizeXY().ToString());
        for(TObjectIterator<USFMinimapPanelWidget> It;It;++It)if(It->GetWorld()==W&&It->IsVisible())
        {
            auto* Prop=FindFProperty<FObjectPropertyBase>(It->GetClass(),TEXT("MinimapMarkerLayer"));auto* Marker=Prop?Cast<UWidget>(Prop->GetObjectPropertyValue_InContainer(*It)):nullptr;
            if(!Marker)continue;
            const auto G=Marker->GetCachedGeometry();const FVector2D Cursor=FSlateApplication::Get().GetCursorPos();const FVector2D Local=G.AbsoluteToLocal(Cursor);
            const FVector2D Position=It->WorldToMinimapPosition(Cam->GetActorLocation());
            Text+=FString::Printf(TEXT("marker_origin=%s\nmarker_size=%s\ncursor_absolute=%s\ncursor_local=%s\ncamera_minimap_local=%s\ncoordinate_error_local_px=%.5f\n"),*G.GetAbsolutePosition().ToString(),*G.GetLocalSize().ToString(),*Cursor.ToString(),*Local.ToString(),*Position.ToString(),FVector2D::Distance(Local,Position));
        }
        const FString Name=Args.IsEmpty()?TEXT("snapshot"):FPaths::GetCleanFilename(Args[0]);const FString Dir=FPaths::ProjectSavedDir()/TEXT("MinimapQA/2026-10-19/native");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Text,*(Dir/(Name+TEXT(".txt"))));
    }));
static FAutoConsoleCommandWithWorldAndArgs Command(TEXT("SF.Minimap.RenderedQA"),TEXT("Run 8 rendered minimap scenarios, 3 repeats; optional output subfolder."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if(!World||!World->IsGameWorld())return;
        Active=MakeShared<FRun>();Active->World=World;Active->Folder=FPaths::ProjectSavedDir()/TEXT("MinimapQA/2026-10-19")/(Args.IsEmpty()?TEXT("rendered"):FPaths::GetCleanFilename(Args[0]));
        IFileManager::Get().MakeDirectory(*Active->Folder,true);
        auto Run=Active;FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float D){return Run->Tick(D);}));
    }));
}
#endif
