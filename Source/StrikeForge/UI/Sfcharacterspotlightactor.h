#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFCharacterSpotlightActor.generated.h"

class USpotLightComponent;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class STRIKEFORGE_API ASFCharacterSpotlightActor : public AActor
{
	GENERATED_BODY()

public:
	ASFCharacterSpotlightActor();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "SF|Spotlight")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "SF|Spotlight")
	USpotLightComponent* SpotLight;

	UPROPERTY(VisibleAnywhere, Category = "SF|Spotlight")
	UNiagaraComponent* SparkleEffect;

	// 스포트라이트 색상 (따뜻한 무대 조명 느낌 기본값)
	UPROPERTY(EditAnywhere, Category = "SF|Spotlight")
	FLinearColor LightColor = FLinearColor(1.0f, 0.95f, 0.8f);

	// 조명 밝기
	UPROPERTY(EditAnywhere, Category = "SF|Spotlight")
	float LightIntensity = 8000.0f;

	// 안쪽/바깥쪽 원뿔 각도 (좁을수록 뚜렷한 스팟 느낌)
	UPROPERTY(EditAnywhere, Category = "SF|Spotlight")
	float InnerConeAngle = 15.0f;
	UPROPERTY(EditAnywhere, Category = "SF|Spotlight")
	float OuterConeAngle = 30.0f;

	// 조명이 닿는 거리
	UPROPERTY(EditAnywhere, Category = "SF|Spotlight")
	float AttenuationRadius = 800.0f;

	// 캐릭터 주변 반짝이는 나이아가라 파티클 에셋 (에디터에서 지정)
	UPROPERTY(EditAnywhere, Category = "SF|Spotlight")
	UNiagaraSystem* SparkleSystemAsset;
};