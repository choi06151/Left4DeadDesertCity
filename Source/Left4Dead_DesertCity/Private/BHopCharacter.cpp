#include "BHopCharacter.h"

#include "BHopCharacterMovementComponent.h"

ABHopCharacter::ABHopCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBHopCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// Keep ticking enabled so auto-jump can work without requiring Blueprint
	// changes. Existing Blueprint Tick logic still runs normally.
	PrimaryActorTick.bCanEverTick = true;
}

void ABHopCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	HandleBHopAutoJump(DeltaSeconds);
}

void ABHopCharacter::Jump()
{
	// Store jump-held state separately because ACharacter clears bPressedJump
	// based on JumpMaxHoldTime. This lets BHop auto-jump work without changing
	// the project's existing jump hold settings.
	bBHopJumpHeld = true;
	Super::Jump();
}

void ABHopCharacter::StopJumping()
{
	bBHopJumpHeld = false;
	Super::StopJumping();
}

UBHopCharacterMovementComponent* ABHopCharacter::GetBHopMovementComponent() const
{
	return Cast<UBHopCharacterMovementComponent>(GetCharacterMovement());
}

void ABHopCharacter::HandleBHopAutoJump(float DeltaSeconds)
{
	(void)DeltaSeconds;

	if (!bEnableBHopAutoJump || !bBHopJumpHeld || !IsLocallyControlled())
	{
		return;
	}

	const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !MoveComp->IsMovingOnGround() || !CanJump())
	{
		return;
	}

	// Re-issue the same standard Character jump request. CharacterMovement will
	// include it in normal prediction/saved moves; no Enhanced Input changes are
	// required.
	Super::Jump();
}
