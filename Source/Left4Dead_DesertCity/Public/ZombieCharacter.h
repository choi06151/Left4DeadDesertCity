#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "ZombieSmartNavLinkProxy.h"
#include "ZombieStatsRow.h"
#include "ZombieCharacter.generated.h"

class AZombieAIController;
class UInputComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZombieMoveCompleted, AZombieCharacter*, ZombieCharacter);

UCLASS()
class LEFT4DEAD_DESERTCITY_API AZombieCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AZombieCharacter();

protected:
	virtual void BeginPlay() override;
	void ApplyStatsRow(const FZombieStatsRow& StatsRow);
	void LoadStatsFromDataTable();
	void StartSimpleStuckCheck();
	void StopSimpleStuckCheck();
	void CheckSimpleStuck();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Zombie|AI")
	void SimpleMove(FVector TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Zombie|AI")
	void QualityMove(AActor* TargetActor);

	/** Move toward an actor while retaining it as the combat target. */
	UFUNCTION(BlueprintCallable, Category = "Zombie|AI")
	void MoveToActor(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Zombie|AI")
	void FinishQualityMoveNavLink();

	void HandleQualityNavLink(const FZombieNavLinkContext& NavLinkContext);

	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|AI")
	void OnQualityMoveNavLink(const FZombieNavLinkContext& NavLinkContext);

	/** Called once when a zombie cannot return to normal navigation after a quality NavLink. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|AI")
	void OnQualityMoveNavLinkRecovery(const FZombieNavLinkContext& NavLinkContext);

	/** Implement montage playback and hit detection in Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|Combat")
	void OnZombieAttack(AActor* TargetActor);

	/** Call from Blueprint after the attack montage/hit window has completely finished. */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Combat")
	void FinishZombieAttack();

	UFUNCTION(BlueprintPure, Category = "Zombie|Combat")
	bool IsZombieAttacking() const { return bIsAttacking; }

	UPROPERTY(BlueprintAssignable, Category = "Zombie|AI")
	FOnZombieMoveCompleted OnMoveCompleted;

	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|Event")
	void ZombieActivateSet();

	UFUNCTION(BlueprintCallable, Category = "Zombie|Event")
	void ZombieDeActivateSet();

	UFUNCTION(BlueprintCallable, Category = "Zombie|Event")
	void ZombieForceReturnToPool();

	/** Resets the pooled/dead state before ZombieActivateSet is dispatched. */
	void PrepareZombieActivation();

	UFUNCTION(BlueprintPure, Category = "Zombie|Event")
	bool IsZombieDeactivated() const { return bIsZombieDeactivated; }

	/** Pauses skeletal animation at its current pose. Spawn automatically resumes it. */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Animation")
	void PauseZombieAnimation();

	UFUNCTION(BlueprintCallable, Category = "Zombie|Movement")
	void LaunchTowardActor(AActor* TargetActor);

	void OnSimpleMoveTargetReached();
	void OnSimpleMoveFinished(EPathFollowingResult::Type ResultCode);
	void OnQualityMoveFinished(EPathFollowingResult::Type ResultCode);

protected:
	void UpdateQualityChase(float DeltaTime);
	void SimpleMoveInternal(FVector TargetLocation, bool bResetRecoveryAttempt);
	void ConfigureActiveCollision();
	void RequestQualityRepath(bool bForceRepath);
	bool ShouldSwitchToSimpleMode(float DistanceToTargetSquared) const;
	float GetQualityRepathIntervalForDistance(float DistanceToTarget) const;
	float GetQualityRepathDistanceForDistance(float DistanceToTarget) const;
	void TriggerTemporaryQualityFromSimpleBlock();
	void HandleSimpleStuck();
	void ResetQualityStuckTracking();
	void HandleQualityStuck(float DeltaTime, float DistanceToTargetSquared);
	void StartZombieAttack(AActor* TargetActor);
	bool TryAttackCurrentTarget();
	void LaunchOutOfQualityStuck();
	bool TryRecoverOffNavAfterNavLink();
	bool TryRelocateFromNavLinkCollision();
	bool IsCapsuleOverlappingBlockingGeometry(const FVector& Location) const;
	bool TriggerQualityNavLinkRecovery();
	void ResetRepeatedNavLinkTracking();
	void DisableZombieActor();
	void ResumeQualityChaseAfterNavLink();
	void OnDespawnTimerExpired();

	FTimerHandle DespawnTimerHandle;
	FTimerHandle SimpleStuckCheckTimerHandle;
	FTimerHandle QualityFailureRetryTimerHandle;

private:
	UPROPERTY()
	TObjectPtr<AZombieAIController> CachedZombieAI;

	UPROPERTY()
	TWeakObjectPtr<AActor> QualityTargetActor;

	FVector LastQualityTargetCenterLocation = FVector::ZeroVector;
	FVector LastQualityGoalLocation = FVector::ZeroVector;
	FVector LastQualityObservedLocation = FVector::ZeroVector;
	FVector LastSimpleObservedLocation = FVector::ZeroVector;
	FVector LastSimpleTargetLocation = FVector::ZeroVector;
	FVector LastNavLinkStartLocation = FVector::ZeroVector;
	FVector LastNavLinkEndLocation = FVector::ZeroVector;
	FZombieNavLinkContext LastNavLinkContext;
	float LastSimpleMoveRequestTime = 0.0f;
	float LastQualityRepathTime = 0.0f;
	float ForcedQualityUntilTime = 0.0f;
	float QualityStuckTime = 0.0f;
	float QualityStuckGraceUntilTime = 0.0f;
	int32 ConsecutiveSimpleFailureCount = 0;
	int32 ConsecutiveSimpleStuckCount = 0;
	int32 SimpleStuckRecoveryAttemptCount = 0;
	int32 RepeatedNavLinkCount = 0;
	bool bIsUsingQualityNavLink = false;
	bool bIgnoreNextQualityMoveFinished = false;
	bool bHasQualityNavLinkContext = false;
	bool bQualityNavLinkRecoveryTriggered = false;
	bool bHasQualityRepathGoal = false;
	bool bIsAttacking = false;
	bool bIsZombieDeactivated = true;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Stats")
	FDataTableRowHandle StatsRowHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float speed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Lifecycle")
	float timeToDisappear = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityEnterDistance = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityExitDistance = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityRepathInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityRepathDistance = 150.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float SimpleStuckCheckInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float SimpleStuckDistanceThreshold = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float SimpleStuckSpeedThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	int32 SimpleStuckCountForRecovery = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float SimpleStuckGracePeriod = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityStuckGracePeriod = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI")
	float QualityStuckMinDistanceToTarget = 60.0f;

	/** Distance at which quality movement stops and Blueprint attack handling begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float ActivationLaunchForwardStrength = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float ActivationLaunchUpStrength = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float NavLinkOffNavRecoveryForwardStrength = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float NavLinkOffNavRecoveryUpStrength = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	FVector NavLinkOffNavProjectionExtent = FVector(80.0f, 80.0f, 120.0f);

	/** Search radius used to relocate a zombie whose capsule is stuck after a NavLink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float NavLinkCollisionRecoveryRadius = 300.0f;

	/** Maximum number of reachable NavMesh locations tested during collision recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (ClampMin = "1"))
	int32 NavLinkCollisionRecoveryAttempts = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float JumpNavLinkFinishForwardNudge = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	float JumpNavLinkEndLocationTolerance = 120.0f;

};
