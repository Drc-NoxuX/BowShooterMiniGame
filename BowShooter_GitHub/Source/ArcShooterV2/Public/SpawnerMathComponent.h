// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpawnerMathComponent.generated.h"

UENUM()
enum class EAxisTypes : uint8
{
	X,
	Y,
	Z
};

USTRUCT()
struct FGridCell
{
	GENERATED_BODY()

	UPROPERTY()
	int32 CellX = 0;
	UPROPERTY()
	int32 CellY = 0;
	UPROPERTY()
	int32 CellZ = 0;

	bool operator==(const FGridCell& Other) const
	{
		return CellX == Other.CellX && CellY == Other.CellY && CellZ == Other.CellZ;
	}
};

FORCEINLINE uint32 GetTypeHash(const FGridCell& Cell)
{
	return HashCombine(GetTypeHash(Cell.CellX), GetTypeHash(Cell.CellY), GetTypeHash(Cell.CellZ));
}

USTRUCT(BlueprintType)
struct FTimeWindow
{
	GENERATED_BODY()
	UPROPERTY()
	float InTime = 0;
	UPROPERTY()
	float OutTime = 0;
	UPROPERTY()
	TWeakObjectPtr<AActor> Owner = nullptr;
};

USTRUCT()
struct FBorderReach
{
	GENERATED_BODY()
	UPROPERTY()
	float Instant = 0;
	UPROPERTY()
	EAxisTypes Axis = EAxisTypes::X;
	UPROPERTY()
	int32 FrontierIndex = 0;
	UPROPERTY()
	bool bIsEntry = false;
	UPROPERTY()
	bool bIsAxisPositive = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCSHOOTERV2_API USpawnerMathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USpawnerMathComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintCallable, Category="Components|SpawnerMath")
	void CalculateCellSize(TSubclassOf<AActor> Target, float SecurityMargin);
	
	UFUNCTION(BlueprintCallable, Category="Components|SpawnerMath")
	void BuildGrid(FVector MinPoint, FVector MaxPoint);
	
	UFUNCTION(BlueprintCallable, Category="Components|SpawnerMath")
	void RegisterActiveObjectTrajectory(AActor* Target);
	
	UFUNCTION()
	void OnActorDestroyed(AActor* Actor);
	
	UFUNCTION(BlueprintCallable, Category="Components|SpawnerMath")
	TArray<FTimeWindow> ComputeForbiddenTimeRanges(TArray<FVector> CheckPoints, float Duration, bool bIsPingPong);
	
	UFUNCTION(BlueprintCallable, Category="Components|SpawnerMath")
	TArray<FTimeWindow> ComputeImmobileForbidden(FVector Position);
	
	UFUNCTION(BlueprintCallable, Category="Components|SpawnerMath")
	float FindValidSpawnTime(const TArray<FTimeWindow>& TimesRange, bool& bOutIsValid);

private:
	UPROPERTY()
	float m_CellSize;
	UPROPERTY()
	FVector m_MinPoint;
	UPROPERTY()
	FVector m_MaxPoint;
	UPROPERTY()
	float m_Radius;
	
	TMap<FGridCell, TArray<FTimeWindow>> CellTimeWindows;
	TMap<TWeakObjectPtr<AActor>, TArray<FGridCell>> ObjectCellList;
	
	//Transforme un trajet en une information exploitable pour la détection de collision spatio-temporelle
	TMap<FGridCell, TArray<FTimeWindow>> ConvertCandidateTrajectory (TArray<FVector> CheckPoints, float Duration);
	TMap<FGridCell, TArray<FTimeWindow>> ConvertImmobileCandidate(FVector Position);
	void ReverseCandidateTrajectory (TArray<FVector> CheckPoints, float Duration, TMap<FGridCell, TArray<FTimeWindow>>& Result);
	
	TArray<int32> CalculateBorder(float Pos0, float Pos1, float MinComponent);
	TArray<FBorderReach> BordersReachByAxes(float Pos0, float Pos1, float MinComponent, float T0, float T1, EAxisTypes Type, const TArray<int32>& Border);
	TArray<int32> InitializeActivePoints(float Pos0, float MinComponent);
	void ModifyActivePoints(const FBorderReach& Reach, TArray<int32>& Active);
	void AddToResult(const TArray<int32>& ActiveX, const TArray<int32>& ActiveY, const TArray<int32>& ActiveZ, TMap<FGridCell, TArray<FTimeWindow>>& Result, float PreviousInstant, float Time);
	TArray<FTimeWindow> MergedTimeRanges(const TMap<FGridCell, TArray<FTimeWindow>>& Result);
	void AppendRepeatingCycles(TMap<FGridCell, TArray<FTimeWindow>>& Result, float CycleLength);
};
