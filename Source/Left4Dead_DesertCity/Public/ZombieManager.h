// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieManager.generated.h"

class AZombieCharacter;

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

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
public:
	void InitializeZombies();
	UFUNCTION(BlueprintCallable)
	void SpawnZombie();
	void DeSpawnZombie(AZombieCharacter* Zombie);
	
	UFUNCTION()
	void OnZombieArrived(AZombieCharacter* ArrivedZombie);

	bool IsSpawnLocationValid(const FVector& CandidateLocation, AZombieCharacter* TargetZombie) const;
	bool IsSpawnLocationReachableToPlayer(const FVector& CandidateLocation) const;
	FVector GetSurroundSlotLocation(const AZombieCharacter* Zombie, const FVector& TargetCenter) const;
private:

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie Setup", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AZombieCharacter> ZombieClassSlot;
	
	UPROPERTY()
	TArray<TWeakObjectPtr<AZombieCharacter>> Zombies;

	UPROPERTY()
	APawn* Player;
public:
	UPROPERTY(VisibleAnywhere,BlueprintReadWrite)
	int32 SpawnEnableZombieCount =2;

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
	
	UPROPERTY()
	int32 CurrentSpawnedZombieCount = 0;
	

};
