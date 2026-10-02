#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFStoryWidget.generated.h"

class SSFStoryScroll;
class UFont;
class UTexture2D;
class ASFStoryIllustrationActor;

DECLARE_MULTICAST_DELEGATE(FSFStoryClosed);

/** A modal world chronicle, painted at a fixed design size and scaled to the viewport. */
UCLASS()
class STRIKEFORGE_API USFStoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    USFStoryWidget(const FObjectInitializer& ObjectInitializer);
    void OpenStory();
    void CloseStory();
    void TurnPage(int32 Direction);
    UTexture2D* GetIllustrationTexture(int32 Index) const;
    bool HasLiveIllustration() const;
    FSFStoryClosed OnClosed;
    int32 GetChapterIndex() const { return ChapterIndex; }
    bool IsClosing() const { return bClosing; }

#if WITH_DEV_AUTOMATION_TESTS
    void PreviewChapter(int32 Index, float OpeningTime = 2.f);
#endif

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
    UPROPERTY()
    TObjectPtr<UFont> StoryFont;
    UPROPERTY() TArray<TObjectPtr<UTexture2D>> IllustrationTextures;
    UPROPERTY(Transient) TObjectPtr<ASFStoryIllustrationActor> IllustrationStage;
    void DestroyIllustration();
    void RefreshPresentation();
    TSharedPtr<SSFStoryScroll> Scroll;
    int32 ChapterIndex = 0;
    float OpeningElapsed = 0.f;
    float PageElapsed = 0.f;
    float ClosingElapsed = 0.f;
    float IllustrationElapsed = 0.f;
    float CloseStartAmount = 1.f;
    bool bClosing = false;
    bool bPreview = false;
};
