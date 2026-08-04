#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartWidget.generated.h"

class UButton;

UCLASS()
class STRIKEFORGE_API UStartWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Start_Btn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Exit_Btn;

	UFUNCTION()
	void OnStartButtonClicked();

	UFUNCTION()
	void OnExitButtonClicked();
};