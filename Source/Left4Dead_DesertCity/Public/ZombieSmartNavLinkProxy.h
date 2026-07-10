#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavLinkProxy.h"
#include "ZombieSmartNavLinkProxy.generated.h"

UENUM(BlueprintType)
enum class EZombieSmartNavLinkType : uint8
{
	Jump,
	Climb
};

USTRUCT(BlueprintType)
struct FZombieNavLinkContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|NavLink")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|NavLink")
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|NavLink")
	FVector MidLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|NavLink")
	FVector MoveDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|NavLink")
	float HeightDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|NavLink")
	float HorizontalDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|NavLink")
	EZombieSmartNavLinkType LinkType = EZombieSmartNavLinkType::Jump;
};

UCLASS()
class LEFT4DEAD_DESERTCITY_API AZombieSmartNavLinkProxy : public ANavLinkProxy
{
	GENERATED_BODY()
};
