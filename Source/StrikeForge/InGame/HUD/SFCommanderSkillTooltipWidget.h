#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFCommanderSkillTooltipWidget.generated.h"

class UTextBlock;

/** Shared by all three skill slots; content follows the currently selected commander. */
UCLASS()
class STRIKEFORGE_API USFCommanderSkillTooltipWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetSkillDetails(const FText& Name, const FText& Effect, const FString& Key,
        int32 Tier, bool bUnlocked, float CooldownSeconds);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> KeyText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EffectText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RequirementText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CooldownText;
};
