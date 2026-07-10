#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZombieStatsRow.generated.h"

USTRUCT(BlueprintType)
struct FZombieStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float MoveSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Lifecycle")
	float TimeToDisappear = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityEnterDistance = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityExitDistance = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityNearDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityMidDistance = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityNearRepathInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityMidRepathInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityFarRepathInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityNearRepathDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityMidRepathDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityFarRepathDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	int32 SimpleFailureCountForQuality = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float TemporaryQualityDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityFailureRetryDelay = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityStuckDistanceThreshold = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityStuckSpeedThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityStuckDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityStuckLaunchForwardStrength = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityStuckLaunchUpStrength = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	int32 MaxRepeatedNavLinkCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float RepeatedNavLinkLocationTolerance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Perception")
	float SightRadius = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Perception")
	float LoseSightRadiusOffset = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Perception")
	float PeripheralVisionAngleDegrees = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Perception")
	float SightMaxAge = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Perception")
	float HearRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|NavLink")
	float ClimbMinHeightDelta = 60.0f;
};
