#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TutorialOverlayWidget.generated.h"

class UImage;
class UButton;
class UCommonTextBlock;

USTRUCT(BlueprintType)
struct FTutorialStep
{
	GENERATED_BODY()

	// Name of the widget to highlight inside the host widget (e.g. "Setting_Btn")
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetWidgetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true))
	FText Message;

	// false: click anywhere to advance (target is NOT clickable)
	// true : player must click the highlighted button itself to advance
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequireTargetClick = false;

	// Extra space around the target inside the spotlight hole
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMargin HolePadding = FMargin(8.f);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTutorialFinished);

/**
 * Full-screen spotlight tutorial overlay.
 * Dims everything except the target widget and shows a message box next to it.
 * Supports both UButton and UCommonButtonBase targets.
 */
UCLASS()
class STRIKEFORGE_API UTutorialOverlayWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Host = the menu widget that owns the target buttons (e.g. WBP_Lobby)
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartTutorial(UUserWidget* InHost);

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void AdvanceStep();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void FinishTutorial();

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnTutorialFinished OnTutorialFinished;

	// Edit steps in the widget blueprint Class Defaults
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	TArray<FTutorialStep> Steps;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// Called when the step changes (play fade / pulse animations in BP)
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial")
	void BP_OnStepEntered(int32 StepIndex);

	// All must sit directly in a root CanvasPanel, anchors (0,0), alignment (0,0)
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> BlockerTop;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> BlockerBottom;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> BlockerLeft;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> BlockerRight;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> HighlightFrame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> MessageBox;      // slot: Size To Content
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> MessageText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ClickCatcher;    // full-screen, transparent style

private:
	void EnterStep(int32 Index);
	void UnbindTarget();
	static void SetRect(UWidget* Widget, const FVector2D& Pos, const FVector2D& Size);

	UFUNCTION() void HandleCatcherClicked();
	UFUNCTION() void HandleTargetClicked();

	TWeakObjectPtr<UUserWidget> HostWidget;
	TWeakObjectPtr<UWidget> CurrentTarget;
	FDelegateHandle TargetClickedHandle; // used only for UCommonButtonBase
	int32 CurrentIndex = INDEX_NONE;
};