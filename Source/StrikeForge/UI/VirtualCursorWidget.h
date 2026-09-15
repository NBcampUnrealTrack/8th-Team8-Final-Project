#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VirtualCursorWidget.generated.h"

class UImage;
class UMouseSensitivitySubsystem;

UCLASS()
class STRIKEFORGE_API UVirtualCursorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP에서 커서 아이콘으로 쓸 Image 위젯 - Hierarchy에서 이름을 반드시 "SFMouse"로 지정할 것
	UPROPERTY(meta = (BindWidget))
	UImage* SFMouse;

private:
	FVector2D CursorPosition = FVector2D::ZeroVector;

	UPROPERTY()
	UMouseSensitivitySubsystem* MouseSensitivitySubsystem;
};