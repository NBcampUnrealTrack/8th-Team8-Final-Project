#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultWidget.generated.h"

class UButton;
class UWidget;

UCLASS()
class STRIKEFORGE_API UResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SF|Result")
	void SetResultState(bool bIsWin);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnExitButtonClicked();

protected:
	UPROPERTY(meta = (BindWidget))
	UWidget* Win_Image;
	UPROPERTY(meta = (BindWidget))
	UWidget* Loose_Image;
	UPROPERTY(meta = (BindWidget))
	UButton* Exit_Btn_W;
	UPROPERTY(meta = (BindWidget))
	UButton* Exit_Btn_F;
};