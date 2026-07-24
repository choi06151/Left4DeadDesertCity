#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BHopCharacter.generated.h"

class UBHopCharacterMovementComponent;

/**
 * Drop-in Character parent for Blueprint players that need Counter-Strike style
 * bunny hopping without changing existing Enhanced Input bindings or Blueprint
 * gameplay logic.
 */
UCLASS(BlueprintType, Blueprintable)
class LEFT4DEAD_DESERTCITY_API ABHopCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABHopCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;
	virtual void Jump() override;
	virtual void StopJumping() override;

	/** Returns the custom BHop movement component installed as CharacterMovement. */
	UFUNCTION(BlueprintPure, Category = "BHop")
	UBHopCharacterMovementComponent* GetBHopMovementComponent() const;

	/** If true, holding the existing Jump input will jump again immediately after landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BHop|Jump")
	bool bEnableBHopAutoJump = true;

protected:
	/** Native Tick only handles optional auto-jump. Movement math stays inside the movement component. */
	void HandleBHopAutoJump(float DeltaSeconds);

	/** Tracks whether the existing Jump input is currently held, independent of ACharacter's short-lived bPressedJump. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "BHop|Jump")
	bool bBHopJumpHeld = false;
};
