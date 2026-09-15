// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFBeMinedable.generated.h"

class ASFMiningBuilding;

UINTERFACE()
class UBeMinedable : public UInterface
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FBeMinedableData
{
	GENERATED_BODY()
	// 자원종류
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EResourceType ResourceType;
	// 남은자원량
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResourceLeftAmount;
	// 자원의 등급
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResourceTier;
	// 채굴시 요구되는 작업량
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ResourceRequiredWork;
	
};

class STRIKEFORGE_API IBeMinedable
{
	GENERATED_BODY()
public:
	virtual int32 Extract(const int32 InDigAmount) = 0;
	
	// getter
	virtual FBeMinedableData GetBeMinedableData() const = 0;
};
