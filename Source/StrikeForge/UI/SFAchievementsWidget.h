#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFAchievementsWidget.generated.h"

class UFont;
class UFontFace;
DECLARE_MULTICAST_DELEGATE(FSFAchievementsClosed);

/** Modal PvE achievement journal, accessible from the campaign lobby. */
UCLASS()
class STRIKEFORGE_API USFAchievementsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    USFAchievementsWidget(const FObjectInitializer& ObjectInitializer);
    UFUNCTION(BlueprintCallable) void CloseAchievements();
    FSFAchievementsClosed OnClosed;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UPROPERTY() TObjectPtr<UFont> TitleFont;
    UPROPERTY() TObjectPtr<UFont> BodyFont;
    UPROPERTY() TObjectPtr<UFontFace> BodyFace;
};
