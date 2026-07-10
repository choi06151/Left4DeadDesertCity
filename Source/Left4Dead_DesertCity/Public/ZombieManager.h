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
	
	UPROPERTY()
	int32 CurrentSpawnedZombieCount = 0;
	

};
