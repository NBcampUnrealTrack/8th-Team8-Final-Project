#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "InGame/HUD/Tutorial/SFStageIntroData.h"
#include "SFTutorialSpotlightWidget.generated.h"

class UTextBlock;
class UWidget;
class UBorder;

/**
 * 화면 한 곳만 밝게 남기고 나머지를 덮는 안내 오버레이.
 *
 * ── 어떻게 쓰는가 ───────────────────────────────────────
 *
 * HUD 가 USFTutorialSubsystem::OnGuideStepStarted 를 구독해서, 받은 Step 을
 * 그대로 ApplyGuideStep 에 넘긴다. OnGuideFinished 에서 ClearGuide 를 부르고
 * 위젯을 지운다. 이 위젯은 안내 진행을 스스로 판단하지 않는다 — 지금 무엇을
 * 기다리는지는 서브시스템만 알고 있으면 된다.
 *
 * ── 구멍은 왜 사각형인가 ────────────────────────────────
 *
 * 덮개를 구멍 바깥 사각형 네 장으로 그린다. 머티리얼이 필요 없어서 에디터
 * 세팅 없이 바로 동작하고, 텍스처도 만들지 않는다. 부드러운 원형 구멍이
 * 필요해지면 DimBrush 에 원형 마스크 머티리얼을 넣으면 된다.
 *
 * ── 입력을 언제 막는가 ──────────────────────────────────
 *
 * 칸의 통과 조건에 따라 매번 다르다.
 *   Click  : 클릭을 받아야 하므로 Visible (아래로 입력이 안 내려간다)
 *   그 외  : 플레이어가 실제로 조작해야 하므로 HitTestInvisible
 *
 * 이 구분이 없으면 "드래그하세요" 안내가 드래그 자체를 막는다.
 */
UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFTutorialSpotlightWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 안내 한 칸을 화면에 올린다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial|Spotlight")
    void ApplyGuideStep(const FSFStageIntroGuideStep& InStep, int32 InStepIndex, int32 InStepCount);

    /** 표시를 지운다. 위젯 자체를 없애는 것은 호출부 몫이다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial|Spotlight")
    void ClearGuide();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    /** 칸이 바뀔 때마다 불린다. 애니메이션을 얹고 싶을 때 쓴다 */
    UFUNCTION(BlueprintImplementableEvent, Category = "SF|Tutorial|Spotlight")
    void OnGuideStepApplied(const FSFStageIntroGuideStep& Step);

    // ── 선택 바인딩 ─────────────────────────────────────
    //
    // 안내 문구는 스포트라이트를 따라가지 않고 WBP 의 CanvasPanel에서
    // 우측 상단에 고정한다. 화면 구성을 바꿀 때는 이 위젯의 슬롯만 조정한다.

    UPROPERTY(BlueprintReadOnly, Category = "SF|Tutorial|Spotlight", meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> InstructionText = nullptr;

    // ── 모양 ────────────────────────────────────────────

    /**
     * 덮개에 쓸 브러시. 비워두면 엔진 기본 흰색 사각형을 쓴다.
     *
     * 부드러운 원형 구멍을 원하면 여기에 머티리얼 브러시를 넣는다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF|Tutorial|Spotlight|Look")
    FSlateBrush DimBrush;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF|Tutorial|Spotlight|Look")
    FLinearColor DimColor = FLinearColor(0.f, 0.f, 0.f, 0.62f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF|Tutorial|Spotlight|Look")
    FLinearColor HighlightColor = FLinearColor(1.f, 0.82f, 0.25f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF|Tutorial|Spotlight|Look", meta = (ClampMin = "1.0"))
    float HighlightThickness = 3.f;

    /** 테두리가 깜빡이는 속도(초당 왕복 횟수). 0 이면 안 깜빡인다 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF|Tutorial|Spotlight|Look", meta = (ClampMin = "0.0"))
    float PulseSpeed = 1.6f;

    /** 깜빡임 폭(0~1). 테두리 두께와 밝기가 함께 움직인다 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF|Tutorial|Spotlight|Look",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PulseAmount = 0.35f;

private:
    void UpdateSkillGuideCard();
    UPROPERTY(Transient)
    TObjectPtr<UBorder> SkillGuideCard = nullptr;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SkillGuideText = nullptr;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> SkillHighlightEdges;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> SkillHighlightGlows;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> SkillSparkles;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSFStage03ProductionTest;
    friend struct FSFCommanderContentPreviewAccess;
#endif
    TMap<TWeakObjectPtr<UWidget>, ESlateVisibility> OriginalGuideVisibility;
    /** 건설 메뉴/건물 카드 단계는 고정 좌표 대신 실제 위젯을 찾는다. */
    UWidget* ResolveScreenTargetWidget() const;
    bool UpdateFromScreenTargetWidget(const FGeometry& OverlayGeometry);

    /** 월드 좌표를 화면 비율로 바꾼다. 화면 밖이면 false */
    bool ProjectStepToScreen(FVector2D& OutCenter, float& OutRadius) const;

    /** 구멍의 반쪽 크기를 픽셀로 돌려준다. 그리기와 문구 배치가 같은 값을 쓴다 */
    FVector2D GetHoleExtentPx(const FVector2D& InSize) const;

    UPROPERTY(Transient)
    FSFStageIntroGuideStep CurrentStep;

    bool bHasStep = false;

    /** 화면 비율(0~1) 기준 구멍. 위젯이 전체 화면을 덮는다는 전제다 */
    FVector2D HoleCenterNorm = FVector2D(0.5f, 0.5f);

    /** 화면 세로 길이에 대한 비율. 가로로 넓어져도 크기가 그대로다 */
    float HoleRadiusNorm = 0.12f;

    /**
     * 축별 반쪽 크기. 0 이면 HoleRadiusNorm 으로 정사각형을 만든다.
     *
     * 가로로 긴 UI 를 감싸려면 정사각형으로는 안 된다.
     */
    FVector2D HoleExtentNorm = FVector2D::ZeroVector;

    /** 대상이 카메라 뒤에 있으면 구멍을 그리지 않고 문구만 남긴다 */
    bool bTargetVisible = true;

    float PulseTime = 0.f;
};
