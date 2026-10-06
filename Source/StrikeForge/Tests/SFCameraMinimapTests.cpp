#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Components/BoxComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "InGame/Controller/SFRTSCameraPawn.h"
#include "InGame/HUD/Minimap/SFMinimapPanelWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCameraMinimapTest, "StrikeForge.Camera.MinimapFocusAndCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCameraMinimapTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    ASFRTSCameraPawn* Camera = World->SpawnActor<ASFRTSCameraPawn>();
    Camera->SetActorLocation(FVector(0, 0, 165));
    Camera->SetStageCameraBounds(FBox(FVector(-1000, -1500, -1000), FVector(1000, 1500, 1000)), 50);

    // Reproduce a long focus sweep hitting a blocking volume on its way to the target.
    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20, 1500, 1000));
    Box->SetCollisionObjectType(ECC_GameTraceChannel2);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();
    Wall->SetActorLocation(FVector(300, 0, 0));
    Camera->FloatingPawnMovement->Velocity = FVector(1500, 100, 0);
    Camera->AddMovementInput(FVector::ForwardVector, 1, true);
    Camera->FocusOnWorldLocation(FVector(800, 1000, -999));
    TestTrue(TEXT("Focus reaches the target without sweeping through intervening walls"),
        Camera->GetActorLocation().Equals(FVector(800, 1000, 165)));
    TestTrue(TEXT("Focus clears residual velocity"), Camera->FloatingPawnMovement->Velocity.IsNearlyZero());
    TestTrue(TEXT("Focus clears queued edge input"), Camera->GetPendingMovementInputVector().IsNearlyZero());
    Camera->FocusOnWorldLocation(FVector(10000, -10000, 0));
    TestTrue(TEXT("Corner focus remains inside both walls including sphere clearance"),
        Camera->GetActorLocation().Equals(FVector(898, -1398, 165)));
    Camera->SetActorLocation(FVector(2000, 0, 165));
    Camera->ClampCameraLocation();
    TestEqual(TEXT("A pawn outside the boundary can recover"), Camera->GetActorLocation().X, 898.0);

    APlayerController* PC = World->SpawnActor<APlayerController>();
    ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    PC->Player = LocalPlayer;
    LocalPlayer->PlayerController = PC;
    World->AddController(PC);
    PC->Possess(Camera);
    USFMinimapPanelWidget* Minimap = NewObject<USFMinimapPanelWidget>(PC);
    Minimap->SetOwningPlayer(PC);
    TestEqual(TEXT("Widget resolves its camera pawn"), Minimap->GetOwningPlayerPawn(), static_cast<APawn*>(Camera));
    Camera->SetEdgeScrollEnabled(true);
    Camera->FloatingPawnMovement->Velocity = FVector(200, 200, 0);
    Minimap->NativeOnMouseEnter(FGeometry(), FPointerEvent());
    TestFalse(TEXT("Hover allows screen-edge scrolling over the minimap"), Camera->bMinimapInteractionActive);
    TestTrue(TEXT("Hover preserves ongoing camera movement"),
        Camera->FloatingPawnMovement->Velocity.Equals(FVector(200, 200, 0)));
    Minimap->bIsMinimapCameraDragging = true;
    Minimap->UpdateCameraInteraction();
    TestTrue(TEXT("A minimap drag suppresses competing edge scrolling"), Camera->bMinimapInteractionActive);
    TestTrue(TEXT("Beginning a drag clears residual velocity"), Camera->FloatingPawnMovement->Velocity.IsNearlyZero());
    Minimap->NativeOnMouseMove(FGeometry(), FPointerEvent());
    TestFalse(TEXT("Releasing the button while still over the minimap restores edge scrolling"), Camera->bMinimapInteractionActive);
    Minimap->bIsMinimapCameraDragging = true;
    Minimap->NativeOnMouseLeave(FPointerEvent());
    TestTrue(TEXT("Dragging outside the minimap still suppresses edge scrolling"), Camera->bMinimapInteractionActive);
    Minimap->NativeOnMouseCaptureLost(FCaptureLostEvent(0, 0));
    TestFalse(TEXT("Lost capture clears drag state"), Minimap->bIsMinimapCameraDragging);
    TestFalse(TEXT("Lost capture outside restores camera input"), Camera->bMinimapInteractionActive);
    Camera->SetEdgeScrollEnabled(false);
    Minimap->NativeOnMouseEnter(FGeometry(), FPointerEvent());
    TestFalse(TEXT("Minimap never overrides the user's edge scroll preference"), Camera->bEnableEdgeScroll);
    World->DestroyWorld(false);
    return true;
}
#endif
