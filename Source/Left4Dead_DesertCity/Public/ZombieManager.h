// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieManager.generated.h"

class AZombieCharacter;
class ATargetPoint;

USTRUCT(BlueprintType)
struct FZombieManagerDebugStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 CurrentWaveIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 DesiredActiveZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 WaveStartBurstCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 WaveActivationBurstCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 WaveRefillBatchSize = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	float WaveRefillInterval = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 ActiveZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 CurrentSpawnedZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 TotalPooledZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 MaxPooledZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 WaitingPooledZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 HiddenPooledZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 AvailablePooledZombieCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Debug")
	int32 DeactivatedAwaitingPoolCount = 0;
};

UCLASS()
class LEFT4DEAD_DESERTCITY_API AZombieManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AZombieManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
public:
	void InitializeZombies();
	UFUNCTION(BlueprintCallable)
	void SpawnZombie();
	void DeSpawnZombie(AZombieCharacter* Zombie);

	UFUNCTION(BlueprintCallable, Category = "Zombie|Wave")
	void SetZombieWaveIndex(int32 WaveIndex);

	/** Sends a gunshot hearing stimulus from the player's current location. */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Perception")
	void ReportPlayerGunshot(float Loudness = 1.0f);

	/** Redirects every active zombie to this world location until ClearZombieMoveFocus is called. */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Movement Focus")
	void SetZombieMoveFocusLocation(FVector TargetLocation);

	/** Redirects every active zombie to the actor, following it while it moves. */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Movement Focus")
	void SetZombieMoveFocusActor(AActor* TargetActor);

	/** Ends movement focus and immediately resumes the normal player chase decision. */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Movement Focus")
	void ClearZombieMoveFocus();

	UFUNCTION(BlueprintPure, Category = "Zombie|Movement Focus")
	bool IsZombieMoveFocusActive() const { return bZombieMoveFocusActive; }

	FVector GetCurrentZombieMoveFocusLocation() const { return GetMoveFocusLocation(); }
	AActor* GetCurrentZombieMoveFocusActor() const { return ZombieMoveFocusActor.Get(); }
	
	UFUNCTION()
	void OnZombieArrived(AZombieCharacter* ArrivedZombie);

	bool IsSpawnLocationValid(const FVector& CandidateLocation, AZombieCharacter* TargetZombie) const;
	bool IsSpawnLocationReachableToPlayer(const FVector& CandidateLocation) const;
	FVector GetSurroundSlotLocation(const AZombieCharacter* Zombie, const FVector& TargetCenter) const;

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetActiveZombieCount() const;

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetTotalPooledZombieCount() const;

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetWaitingPooledZombieCount() const;

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetHiddenPooledZombieCount() const;

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetAvailablePooledZombieCount() const;

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetDeactivatedAwaitingPoolCount() const;

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetCurrentSpawnedZombieCount() const { return CurrentSpawnedZombieCount; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	int32 GetDesiredActiveZombieCount() const { return DesiredActiveZombieCount; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Debug")
	FZombieManagerDebugStats GetZombieDebugStats() const;

	void NotifyZombieDeactivated(AZombieCharacter* Zombie);

private:
	void SpawnZombieInternal(bool bLaunchOnActivation);
	void EnsureZombiePoolSize(int32 DesiredPoolSize);
	AZombieCharacter* TakeAvailablePooledZombie(bool bAllowForceRecycleDeactivated);
	bool FindSpawnLocationAroundPlayer(AZombieCharacter* TargetZombie, FVector& OutSpawnLocation) const;
	bool IsActiveZombie(const AZombieCharacter* Zombie) const;
	bool IsAvailablePooledZombie(const AZombieCharacter* Zombie) const;
	int32 TrimActiveZombiesToDesiredCount();
	void ApplyMoveFocusToActiveZombies(bool bForceRequest);
	void ApplyWaveSpeedBoostToActiveZombies();
	void MaintainActorMoveFocus();
	void ResumeNormalMovement(AZombieCharacter* Zombie);
	FVector GetMoveFocusLocation() const;
	void MaintainZombiePopulation(int32 MaxSpawnCount = 1, bool bLaunchOnActivation = false);
	int32 ResolveDesiredActiveZombieCount(int32 WaveIndex) const;
	void SchedulePoolWarmup();
	void WarmupZombiePool();
	void CollectInitialSpawnTargetPoints();
	void AdoptLevelPlacedZombies();
	bool TrySpawnInitialWaitingZombie();
	bool ActivateInitialWaitingZombie(bool bIgnoreActivationDistance = false);
	bool ActivateZombie(AZombieCharacter* Zombie, bool bLaunchOnActivation);

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie Setup", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AZombieCharacter> ZombieClassSlot;
	
	UPROPERTY()
	TArray<TWeakObjectPtr<AZombieCharacter>> Zombies;

	/** TargetPoints collected from the level and sorted by actor name. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Zombie Setup", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<ATargetPoint>> InitialSpawnTargetPoints;

	/** Pooled zombies that are hidden until a wave places them around the player. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AZombieCharacter>> InitialWaitingZombies;

	/** Deactivated corpses waiting for their normal despawn timer. Oldest entries are recycled first if a wave needs bodies immediately. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AZombieCharacter>> DeactivatedZombiesAwaitingPool;

	int32 NextInitialSpawnTargetPointIndex = 0;

	UPROPERTY()
	APawn* Player;

	FTimerHandle PoolWarmupTimerHandle;
	float WaveRefillElapsedTime = 0.0f;
	float LastManualSpawnRequestTime = -FLT_MAX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Movement Focus", meta = (AllowPrivateAccess = "true"))
	bool bZombieMoveFocusActive = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> ZombieMoveFocusActor;

	FVector ZombieMoveFocusLocation = FVector::ZeroVector;
	FVector LastAppliedMoveFocusLocation = FVector::ZeroVector;
public:
	/** Minimum actor movement required before active zombie paths are refreshed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement Focus", meta = (ClampMin = "1.0"))
	float MoveFocusRefreshDistance = 30.0f;

	/** When an actor moves away after a zombie has arrived, chasing resumes past this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement Focus", meta = (ClampMin = "30.0"))
	float MoveFocusResumeDistance = 75.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Setup", meta = (ClampMin = "1"))
	int32 InitialPooledZombieCount = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Setup", meta = (ClampMin = "1"))
	int32 MaxPooledZombieCount = 30;

	/** Height added to TargetPoint locations for the initial idle placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Setup")
	float InitialSpawnHeightOffset = 100.0f;

	/** Only initial idle zombies within this 2D distance of the player join a wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Setup", meta = (ClampMin = "0.0"))
	float InitialIdleActivationDistance = 3000.0f;

	/** Initial idle zombies inside this distance can react directly to a gunshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Perception", meta = (ClampMin = "0.0"))
	float GunshotIdleAlertDistance = 15000.0f;

	/** Maximum number of initial idle zombies awakened by a single gunshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Perception", meta = (ClampMin = "0"))
	int32 MaxIdleGunshotResponders = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Wave")
	int32 CurrentWaveIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie Wave")
	int32 DesiredActiveZombieCount = 0;

	/** Legacy burst setting kept for Blueprint compatibility; wave target count is resolved from CurrentWaveIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Wave", meta = (ClampMin = "0"))
	int32 WaveStartBurstCount = 10;

	/** Number of zombies placed immediately when a wave starts. Remaining target count is refilled over time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Wave", meta = (ClampMin = "0"))
	int32 WaveActivationBurstCount = 10;

	/** Seconds between wave refill batches after the initial activation burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Wave", meta = (ClampMin = "0.01"))
	float WaveRefillInterval = 0.5f;

	/** Number of zombies added on each wave refill tick while active count is below the wave target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Wave", meta = (ClampMin = "1"))
	int32 WaveRefillBatchSize = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Wave", meta = (ClampMin = "1.0"))
	float WaveSpeedBoostMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Wave", meta = (ClampMin = "0.0"))
	float WaveSpeedBoostDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Setup", meta = (ClampMin = "0.01"))
	float PoolWarmupInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Setup", meta = (ClampMin = "1"))
	int32 PoolWarmupBatchSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Spawn")
	float SpawnMinDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Spawn")
	float SpawnMaxDistance = 5500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Spawn")
	int32 SpawnMaxAttempts = 20;

	/** Radius of the first ring of stable zombie slots around a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Surround")
	float SurroundBaseRadius = 140.0f;

	/** Additional radius for every full group of SurroundSlotsPerRing zombies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Surround")
	float SurroundRingSpacing = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Surround", meta = (ClampMin = "1"))
	int32 SurroundSlotsPerRing = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Surround")
	FVector SurroundNavProjectionExtent = FVector(80.0f, 80.0f, 200.0f);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Debug", meta = (AllowPrivateAccess = "true"))
	int32 CurrentSpawnedZombieCount = 0;
	

};
