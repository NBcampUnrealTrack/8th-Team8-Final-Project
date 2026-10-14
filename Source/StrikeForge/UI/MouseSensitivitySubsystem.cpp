#include "UI/MouseSensitivitySubsystem.h"
#include "AudioDevice.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/ICursor.h"
#include "Slate/SceneViewport.h"

void UMouseSensitivitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GConfig->GetFloat(TEXT("Input"), TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("Audio"), TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, MinSensitivity, MaxSensitivity);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
}

void UMouseSensitivitySubsystem::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);
	if (UWorld* World = GetWorld())
		if (FAudioDeviceHandle Device = World->GetAudioDevice()) Device->SetTransientPrimaryVolume(MasterVolume);
	GConfig->SetFloat(TEXT("Audio"), TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UMouseSensitivitySubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	if (AudioWorld.Get() != World)
	{
		if (FAudioDeviceHandle Device = World->GetAudioDevice())
		{
			Device->SetTransientPrimaryVolume(MasterVolume);
			AudioWorld = World;
		}
		CursorMotion.Reset();
	}
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	UGameViewportClient* Viewport = World->GetGameViewport();
	if (!PC || !PC->bShowMouseCursor || !Viewport || !Viewport->Viewport ||
		!Viewport->Viewport->HasFocus() || !FSlateApplication::IsInitialized())
	{
		CursorMotion.Reset();
		return;
	}
	FSceneViewport* SceneViewport = Viewport->GetGameViewport();
	const TSharedPtr<ICursor> Cursor = FSlateApplication::Get().GetPlatformCursor();
	if (!SceneViewport || !Cursor.IsValid()) { CursorMotion.Reset(); return; }
	const FGeometry& Geometry = SceneViewport->GetCachedGeometry();
	const FVector2D Origin = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D Size = Geometry.LocalToAbsolute(Geometry.GetLocalSize()) - Origin;
	const FVector2D Position = Cursor->GetPosition();
	if (Origin != LastViewportOrigin || Size != LastViewportSize)
	{
		CursorMotion.Reset();
		LastViewportOrigin = Origin;
		LastViewportSize = Size;
	}
	if (Size.X < 1. || Size.Y < 1. || Position.X < Origin.X || Position.Y < Origin.Y ||
		Position.X >= Origin.X + Size.X || Position.Y >= Origin.Y + Size.Y)
	{
		CursorMotion.Reset();
		return;
	}
	const FVector2D Requested = CursorMotion.Adjust(Position, MouseSensitivity);
	if (Requested == Position)
	{
		CursorMotion.RecordActualPosition(Position);
		return;
	}
	const FVector2D Target(
		FMath::Clamp(Requested.X, FMath::CeilToDouble(Origin.X), FMath::FloorToDouble(Origin.X + Size.X - 1.)),
		FMath::Clamp(Requested.Y, FMath::CeilToDouble(Origin.Y), FMath::FloorToDouble(Origin.Y + Size.Y - 1.)));
	if (Requested != Target) CursorMotion.Remainder = FVector2D::ZeroVector;
	if (Target != Position) FSlateApplication::Get().SetCursorPos(Target);
	// Read back the actual OS position: clipping/rounding must not become next-frame input.
	CursorMotion.RecordActualPosition(Cursor->GetPosition());
}

void UMouseSensitivitySubsystem::SetMouseSensitivity(float NewSensitivity)
{
	MouseSensitivity = FMath::Clamp(NewSensitivity, MinSensitivity, MaxSensitivity);
	CursorMotion.Reset();
	GConfig->SetFloat(TEXT("Input"), TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
