#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFHealthBarWidget.generated.h"

class UProgressBar;

/**
 * 체력 바 한 줄.
 *
 * 자기가 그릴 값만 받는다. 상태 구조체를 통째로 받지 않는다 —
 * 진행도 같은 남의 필드를 알 이유가 없고, 알면 남의 변화에도 깨어난다.
 *
 * 값이 실제로 바뀌었는지 판단하는 일은 판(USFOverheadPanelWidget)이 한다.
 * 그래서 여기에는 이전 값 캐시가 없다.
 */
UCLASS()
class STRIKEFORGE_API USFHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRatio(float InRatio);
	void SetColor(const FLinearColor& InColor);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
};
