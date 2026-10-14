#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFMenuAtmosphereWidget.generated.h"

class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Decorative only: never takes input or owns any menu navigation. */
UCLASS()
class STRIKEFORGE_API USFMenuAtmosphereWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	USFMenuAtmosphereWidget(const FObjectInitializer& ObjectInitializer);
	void SetDeparture(bool bInDeparture);
	bool IsReady() const { return Material != nullptr; }
	// Keep the departure normalization in LobbyAtmosphere.ush in sync.
	static constexpr float DepartureDuration = 0.62f;
	float GetPresentationTime() const
	{
#if WITH_DEV_AUTOMATION_TESTS
		return PreviewTime.Get(Elapsed);
#else
		return Elapsed;
#endif
	}
#if WITH_DEV_AUTOMATION_TESTS
	void SetPreviewTime(TOptional<float> Time);
#endif

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Hard reference also ensures both the material and its textures are cooked.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> AtmosphereMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
	float Elapsed = 0.f;
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<float> PreviewTime;
#endif
};
