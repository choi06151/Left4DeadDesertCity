#include "BHopCharacterMovementComponent.h"

#include "GameFramework/Character.h"

UBHopCharacterMovementComponent::UBHopCharacterMovementComponent()
{
	// These defaults keep standard CharacterMovement behavior available, while
	// making the falling state friendly to Source-style air acceleration.
	AirControl = 0.0f;
	FallingLateralFriction = 0.0f;
	BrakingDecelerationFalling = 0.0f;
}

void UBHopCharacterMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Landing grace is local movement state derived from movement mode and
	// velocity. It is simulated on client and server through CharacterMovement.
	if (LandingFrictionGraceRemaining > 0.0f)
	{
		LandingFrictionGraceRemaining = FMath::Max(0.0f, LandingFrictionGraceRemaining - DeltaTime);
	}
}

float UBHopCharacterMovementComponent::GetMaxSpeed() const
{
	const float BaseMaxSpeed = Super::GetMaxSpeed();
	if (!bEnableBunnyHop)
	{
		return BaseMaxSpeed;
	}

	// Falling must not be capped by MaxWalkSpeed, otherwise air strafing cannot
	// build or preserve BHop speed.
	if (IsFalling())
	{
		return FMath::Max(BaseMaxSpeed, MaxBHopSpeed);
	}

	// Right after landing, allow the current BHop velocity to survive long
	// enough for the next jump input. Outside this short window, walking returns
	// to the normal MaxWalkSpeed cap.
	if (ShouldUseBHopLandingFriction())
	{
		return FMath::Max(BaseMaxSpeed, MaxBHopSpeed);
	}

	return BaseMaxSpeed;
}

void UBHopCharacterMovementComponent::CalcVelocity(
	float DeltaTime,
	float Friction,
	bool bFluid,
	float BrakingDeceleration)
{
	if (!bEnableBunnyHop)
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}

	// While falling, replace UE's max-speed-clamped air control with Source-style
	// AirAccelerate. The rest of PhysFalling still handles gravity, collision,
	// landing checks, root motion, and prediction.
	if (IsFalling())
	{
		if (!HasValidData() ||
			HasAnimRootMotion() ||
			CurrentRootMotion.HasOverrideVelocity() ||
			DeltaTime < MIN_TICK_TIME ||
			(CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
		{
			return;
		}

		if (HasBHopMovementInput())
		{
			ApplySourceAirAcceleration(DeltaTime);
		}
		else
		{
			ApplyNoInputAirBraking(DeltaTime);
		}

		if (bFluid)
		{
			Velocity *= 1.0f - FMath::Min(FMath::Max(0.0f, Friction) * DeltaTime, 1.0f);
		}

		ClampHorizontalVelocity(MaxBHopSpeed);
		return;
	}

	// During the small post-landing window, use low friction so speed is not
	// destroyed before the next jump. If there is no movement input, use normal
	// walking friction so the player can stop cleanly.
	if (ShouldUseBHopLandingFriction())
	{
		Super::CalcVelocity(
			DeltaTime,
			FMath::Min(Friction, BHopLandingGroundFriction),
			bFluid,
			FMath::Min(BrakingDeceleration, BHopLandingBrakingDeceleration));
		ClampHorizontalVelocity(MaxBHopSpeed);
		return;
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

FVector UBHopCharacterMovementComponent::GetFallingLateralAcceleration(float DeltaTime)
{
	if (!bEnableBunnyHop)
	{
		return Super::GetFallingLateralAcceleration(DeltaTime);
	}

	// Source-style air acceleration uses raw wish direction from player input.
	// Do not scale it by UE AirControl; AirControl remains available for other
	// movement modes but is not the BHop acceleration model.
	return GetHorizontalComponent(Acceleration).GetClampedToMaxSize(GetMaxAcceleration());
}

void UBHopCharacterMovementComponent::OnMovementModeChanged(
	EMovementMode PreviousMovementMode,
	uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (!bEnableBunnyHop)
	{
		LandingFrictionGraceRemaining = 0.0f;
		return;
	}

	// Start a short low-friction window only when we actually landed with enough
	// horizontal speed. This avoids making normal walking feel floaty.
	if (PreviousMovementMode == MOVE_Falling && IsMovingOnGround())
	{
		const float HorizontalSpeed = GetBHopHorizontalSpeed();
		LandingFrictionGraceRemaining = HorizontalSpeed >= MinBHopLandingSpeed
			? BHopLandingFrictionGraceTime
			: 0.0f;
	}
	else if (MovementMode == MOVE_Falling)
	{
		LandingFrictionGraceRemaining = 0.0f;
	}
}

float UBHopCharacterMovementComponent::GetBHopHorizontalSpeed() const
{
	return GetHorizontalComponent(Velocity).Size();
}

void UBHopCharacterMovementComponent::ApplySourceAirAcceleration(float DeltaTime)
{
	const FVector WishVelocity = GetHorizontalComponent(Acceleration);
	const FVector WishDirection = WishVelocity.GetSafeNormal();
	if (WishDirection.IsNearlyZero())
	{
		return;
	}

	// Source AirAccelerate:
	// currentSpeed = dot(velocity, wishDir)
	// addSpeed     = wishSpeed - currentSpeed
	// accelSpeed   = accel * wishSpeed * DeltaTime
	// velocity    += min(accelSpeed, addSpeed) * wishDir
	const float AnalogScale = FMath::Clamp(GetAnalogInputModifier(), 0.0f, 1.0f);
	const float WishSpeed = FMath::Min(AirWishSpeed * FMath::Max(AnalogScale, 0.01f), MaxBHopSpeed);
	const float CurrentSpeed = FVector::DotProduct(GetHorizontalComponent(Velocity), WishDirection);
	const float AddSpeed = WishSpeed - CurrentSpeed;
	if (AddSpeed <= 0.0f)
	{
		return;
	}

	const float AccelSpeed = FMath::Min(SourceAirAcceleration * WishSpeed * DeltaTime, AddSpeed);
	Velocity += WishDirection * AccelSpeed;
}

void UBHopCharacterMovementComponent::ApplyNoInputAirBraking(float DeltaTime)
{
	if (AirNoInputFriction <= 0.0f && AirNoInputBrakingDeceleration <= 0.0f)
	{
		return;
	}

	const FVector HorizontalVelocity = GetHorizontalComponent(Velocity);
	const FVector VerticalVelocity = Velocity - HorizontalVelocity;

	Velocity = HorizontalVelocity;
	ApplyVelocityBraking(DeltaTime, AirNoInputFriction, AirNoInputBrakingDeceleration);
	Velocity = GetHorizontalComponent(Velocity) + VerticalVelocity;
}

void UBHopCharacterMovementComponent::ClampHorizontalVelocity(float MaxHorizontalSpeed)
{
	if (MaxHorizontalSpeed <= 0.0f)
	{
		return;
	}

	const FVector HorizontalVelocity = GetHorizontalComponent(Velocity);
	const float HorizontalSpeed = HorizontalVelocity.Size();
	if (HorizontalSpeed <= MaxHorizontalSpeed)
	{
		return;
	}

	const FVector VerticalVelocity = Velocity - HorizontalVelocity;
	Velocity = HorizontalVelocity.GetSafeNormal() * MaxHorizontalSpeed + VerticalVelocity;
}

FVector UBHopCharacterMovementComponent::GetHorizontalComponent(const FVector& Vector) const
{
	return ProjectToGravityFloor(Vector);
}

bool UBHopCharacterMovementComponent::HasBHopMovementInput() const
{
	return GetHorizontalComponent(Acceleration).SizeSquared() >
		FMath::Square(FMath::Max(0.0f, MinInputAccelerationForBHop));
}

bool UBHopCharacterMovementComponent::ShouldUseBHopLandingFriction() const
{
	if (!bEnableBunnyHop || LandingFrictionGraceRemaining <= 0.0f || !IsMovingOnGround())
	{
		return false;
	}

	if (bRequireInputForLandingSpeedPreservation && !HasBHopMovementInput())
	{
		return false;
	}

	return GetBHopHorizontalSpeed() > Super::GetMaxSpeed();
}
