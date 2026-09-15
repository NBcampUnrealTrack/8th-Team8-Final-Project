#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MouseSensitivitySubsystem.generated.h"

UCLASS()
class STRIKEFORGE_API UMouseSensitivitySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 감도 값을 설정 (슬라이더 OnValueChanged에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Mouse Settings")
	void SetMouseSensitivity(float NewSensitivity);

	// 현재 감도 값을 가져옴 (가상 커서 Tick에서 호출)
	UFUNCTION(BlueprintPure, Category = "Mouse Settings")
	float GetMouseSensitivity() const { return MouseSensitivity; }

protected:
	// 기본값 1.0, 클램프 범위는 프로젝트 기획에 맞게 조정
	UPROPERTY(BlueprintReadOnly, Category = "Mouse Settings")
	float MouseSensitivity = 1.0f;

	static constexpr float MinSensitivity = 0.1f;
	static constexpr float MaxSensitivity = 3.0f;
};