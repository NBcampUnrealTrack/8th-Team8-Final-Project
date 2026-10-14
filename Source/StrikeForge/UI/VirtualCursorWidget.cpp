#include "VirtualCursorWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/MouseSensitivitySubsystem.h" // 실제 경로에 맞게 수정

void UVirtualCursorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		MouseSensitivitySubsystem = GI->GetSubsystem<UMouseSensitivitySubsystem>();
	}

	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		FVector2D ViewportSize;
		Viewport->GetViewportSize(ViewportSize);
		CursorPosition = ViewportSize * 0.5f;
	}
}

void UVirtualCursorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	APlayerController* PC = GetOwningPlayer();
	if (!PC || !IsValid(SFMouse))
	{
		return;
	}

	float DeltaX = 0.f, DeltaY = 0.f;
	PC->GetInputMouseDelta(DeltaX, DeltaY);

	const float Sensitivity = MouseSensitivitySubsystem ? MouseSensitivitySubsystem->GetMouseSensitivity() : 1.0f;

	CursorPosition.X += DeltaX * Sensitivity;
	CursorPosition.Y -= DeltaY * Sensitivity;

	FVector2D ViewportSize(1.f, 1.f);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->GetViewportSize(ViewportSize);
	}

	CursorPosition.X = FMath::Clamp(CursorPosition.X, 0.f, ViewportSize.X);
	CursorPosition.Y = FMath::Clamp(CursorPosition.Y, 0.f, ViewportSize.Y);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SFMouse->Slot))
	{
		CanvasSlot->SetPosition(CursorPosition);
	}
}