// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DetourCrowdAIController.h"
#include "ZombieStatsRow.h"
#include "Navigation/PathFollowingComponent.h" 
#include "Perception/AIPerceptionTypes.h"

#include "ZombieAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;


/**
 * 
 */

UENUM(BlueprintType)
enum class EZombieAIMode : uint8
{
	Simple,
	Quality
};

UCLASS()
class LEFT4DEAD_DESERTCITY_API AZombieAIController : public ADetourCrowdAIController
{
	GENERATED_BODY()
	

public:
	AZombieAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;	

	
	UFUNCTION(BlueprintCallable, Category = "ZombieAI")
	void SetAIMode(EZombieAIMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "ZombieAI")
	void SetPerceptionEnabled(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "ZombieAI")
	EZombieAIMode GetCurrentMode() const { return CurrentMode; }

	void SetCrowdAvoidanceEnabled(bool bEnable);
	void ApplyStatsRow(const FZombieStatsRow& StatsRow);

	
	
protected:
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, Category = "ZombieAI")
	EZombieAIMode CurrentMode;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> ZombiePerceptionComponent;

	// 👁️ 시야 및 👂 청각 설정 클래스
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	bool bPerceptionEnabled = false;

	// 무언가 감지되었을 때 호출될 콜백 함수
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float sightRadius=700.f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float hearRadius=900.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float loseSightRadiusOffset = 500.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float peripheralVisionAngleDegrees = 60.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float sightMaxAge = 5.0f;
};
