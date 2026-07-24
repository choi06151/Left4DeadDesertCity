#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BHopCharacterMovementComponent.generated.h"

/**
 * CharacterMovementComponent extension that keeps UE's default walking/falling
 * simulation, but swaps the falling velocity update for Source-style air
 * acceleration. This keeps standard Character prediction, collision, floor
 * checks, root motion handling, and replicated movement paths intact.
 */
UCLASS(BlueprintType, Blueprintable)
class LEFT4DEAD_DESERTCITY_API UBHopCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBHopCharacterMovementComponent();

	/** Allows falling and landing logic to use BHop movement. Disable to return to normal CharacterMovement behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop", meta = (DisplayName = "Enable Bunny Hop"))
	bool bEnableBunnyHop = true;

	/** Maximum horizontal speed allowed while bunny hopping. Normal walking still uses MaxWalkSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Speed", meta = (ClampMin = "1.0"))
	float MaxBHopSpeed = 2200.0f;

	/** Source-style air acceleration scale. Higher values build speed faster while strafing in air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Air", meta = (ClampMin = "0.0"))
	float SourceAirAcceleration = 14.0f;

	/** Desired air movement speed used by the Source AirAccelerate formula. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Air", meta = (ClampMin = "1.0"))
	float AirWishSpeed = 600.0f;

	/** Horizontal braking used only when falling with no movement input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Air", meta = (ClampMin = "0.0"))
	float AirNoInputBrakingDeceleration = 80.0f;

	/** Air friction used only when falling with no movement input. Keep very low for CS-style air speed preservation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Air", meta = (ClampMin = "0.0"))
	float AirNoInputFriction = 0.05f;

	/** Ground friction used briefly after landing at BHop speed, so speed is not instantly killed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Landing", meta = (ClampMin = "0.0"))
	float BHopLandingGroundFriction = 0.35f;

	/** Ground braking used briefly after landing at BHop speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Landing", meta = (ClampMin = "0.0"))
	float BHopLandingBrakingDeceleration = 96.0f;

	/** Time window after landing where high horizontal speed can be preserved for a follow-up jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Landing", meta = (ClampMin = "0.0"))
	float BHopLandingFrictionGraceTime = 0.12f;

	/** Minimum landing horizontal speed required to enable the low-friction landing grace window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Landing", meta = (ClampMin = "0.0"))
	float MinBHopLandingSpeed = 650.0f;

	/** If true, landing grace only preserves speed while the player is still giving movement input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Landing")
	bool bRequireInputForLandingSpeedPreservation = true;

	/** Input acceleration smaller than this is treated as no movement input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Input", meta = (ClampMin = "0.0"))
	float MinInputAccelerationForBHop = 1.0f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual FVector GetFallingLateralAcceleration(float DeltaTime) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** Returns true while landing grace is active and speed preservation can affect ground movement. */
	UFUNCTION(BlueprintPure, Category = "BHop")
	bool IsBHopLandingGraceActive() const { return LandingFrictionGraceRemaining > 0.0f; }

	/** Current horizontal speed measured on the plane perpendicular to gravity. */
	UFUNCTION(BlueprintPure, Category = "BHop")
	float GetBHopHorizontalSpeed() const;

protected:
	/** Implements the Source Engine AirAccelerate step using Velocity, wish direction, wish speed, and DeltaTime. */
	void ApplySourceAirAcceleration(float DeltaTime);

	/** Applies small horizontal braking only when airborne without movement input. */
	void ApplyNoInputAirBraking(float DeltaTime);

	/** Clamps only horizontal velocity, leaving vertical/gravity velocity untouched. */
	void ClampHorizontalVelocity(float MaxHorizontalSpeed);

	/** Returns the horizontal component of a world-space vector relative to the current gravity direction. */
	FVector GetHorizontalComponent(const FVector& Vector) const;

	/** True when the current input acceleration is strong enough to count as intentional movement. */
	bool HasBHopMovementInput() const;

	/** True when walking should temporarily use low BHop landing friction instead of normal ground friction. */
	bool ShouldUseBHopLandingFriction() const;

private:
	/** Counts down after a high-speed landing to prevent instant ground friction loss. */
	float LandingFrictionGraceRemaining = 0.0f;
};
