// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZombieCharacter.generated.h"

class AZombieAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZombieMoveCompleted, AZombieCharacter*, ZombieCharacter);

UCLASS()
class LEFT4DEAD_DESERTCITY_API AZombieCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AZombieCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
public:
	
	UFUNCTION(BlueprintCallable, Category = "Zombie|AI")
	void SimpleMove(FVector TargetLocation);
	UFUNCTION(BlueprintCallable, Category = "Zombie|AI")
	void QualityMove(AActor* TargetActor);
	
	
	// 2. 블루프린트에서 assign 가능한 델리게이트 변수
	UPROPERTY(BlueprintAssignable, Category = "Zombie|AI")
	FOnZombieMoveCompleted OnMoveCompleted;
	
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|Event")
	void ZombieActivateSet();
	
	UFUNCTION(BlueprintCallable, Category = "Zombie|Event")
	void ZombieDeActivateSet();
	
	
	
	
	void OnSimpleMoveTargetReached();
	
protected:
	// 지정된 시간이 지난 후 매니저의 Despawn을 호출할 내부 함수
	void OnDespawnTimerExpired();

	// 타이머 관리를 위한 핸들 변수
	FTimerHandle DespawnTimerHandle;
	
private:
	// 나의 AI 컨트롤러를 기억해둘 캐싱 변수
	UPROPERTY()
	TObjectPtr<AZombieAIController> CachedZombieAI;
	
public:
	UPROPERTY(BlueprintReadWrite)
	float speed=200.0f;
	
	UPROPERTY(BlueprintReadWrite)
	float timeToDisappear=10.0f;
	
	AZombieAIController* AI;
	

};
