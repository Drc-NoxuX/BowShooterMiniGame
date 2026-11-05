// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnerMathComponent.h"

#include "Components/InterpToMovementComponent.h"
#include "GeometryCollection/GeometryCollectionParticlesData.h"

// Sets default values for this component's properties
USpawnerMathComponent::USpawnerMathComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void USpawnerMathComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


// Called every frame
void USpawnerMathComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void USpawnerMathComponent::CalculateCellSize(TSubclassOf<AActor> Target, float SecurityMargin)
{
	FVector Origin;
	FVector BoxExtent;

	Target.GetDefaultObject()->GetActorBounds(false, Origin, BoxExtent, false);

	float MinimumRadius = BoxExtent.Size();
	m_Radius = MinimumRadius + SecurityMargin;

	m_CellSize = m_Radius * 2;
}

void USpawnerMathComponent::BuildGrid(FVector MinPoint, FVector MaxPoint)
{
	m_MinPoint = MinPoint;
	m_MaxPoint = MaxPoint;
}

TArray<int32> USpawnerMathComponent::CalculateBorder(float Pos0, float Pos1, float MinComponent)
{
	float MinPos = FMath::Min(Pos0, Pos1);
	float MaxPos = FMath::Max(Pos0, Pos1);
	int32 MinCell = FMath::TruncToInt32((MinPos - MinComponent) / m_CellSize);
	int32 MaxCell = FMath::TruncToInt32((MaxPos - MinComponent) / m_CellSize);

	TArray<int32> Borders;
	if (!FMath::IsNearlyEqual(Pos0, Pos1, 1E-08))
	{
		for (int32 n = MinCell; n <= MaxCell; n++)
		{
			Borders.Add(n);
		}
	}

	return Borders;
}

TArray<FBorderReach> USpawnerMathComponent::BordersReachByAxes(float Pos0, float Pos1, float MinComponent, float T0,
                                                               float T1, EAxisTypes Type, const TArray<int32>& Border)
{
	TArray<FBorderReach> Entries;

	bool bIsAxisPositive = false;
	if (Pos0 <= Pos1)
	{
		bIsAxisPositive = true;
	}

	for (int32 i : Border)
	{
		float BorderPos = i * m_CellSize + MinComponent;
		float TEntry = FMath::Clamp((T0 + ((BorderPos - m_Radius) - Pos0) * (T1 - T0) / (Pos1 - Pos0)), T0, T1);
		float TExit = FMath::Clamp((T0 + ((BorderPos + m_Radius) - Pos0) * (T1 - T0) / (Pos1 - Pos0)), T0, T1);


		FBorderReach Entry;
		Entry.Axis = Type;
		Entry.FrontierIndex = i;
		Entry.bIsEntry = true;
		Entry.Instant = TEntry;
		Entry.bIsAxisPositive = bIsAxisPositive;
		Entries.Add(Entry);

		FBorderReach Exit;
		Exit.Axis = Type;
		Exit.FrontierIndex = i;
		Exit.bIsEntry = false;
		Exit.Instant = TExit;
		Exit.bIsAxisPositive = bIsAxisPositive;
		Entries.Add(Exit);
	}

	return Entries;
}

TArray<int32> USpawnerMathComponent::InitializeActivePoints(float Pos0, float MinComponent)
{
	TArray<int32> Active;
	int32 BaseCell = FMath::TruncToInt32((Pos0 - MinComponent) / m_CellSize);
	Active.Add(BaseCell);
	float RightBorder = (BaseCell + 1) * m_CellSize + MinComponent;
	if (Pos0 + m_Radius > RightBorder)
	{
		Active.Add(BaseCell + 1);
	}
	float LeftBorder = (BaseCell) * m_CellSize + MinComponent;
	if (Pos0 - m_Radius < LeftBorder)
	{
		Active.Add(BaseCell - 1);
	}

	return Active;
}

void USpawnerMathComponent::ModifyActivePoints(const FBorderReach& Reach, TArray<int32>& Active)
{
	if (Reach.bIsAxisPositive && Reach.bIsEntry)
	{
		Active.Add(Reach.FrontierIndex);
	}
	else if (Reach.bIsAxisPositive)
	{
		Active.Remove(Reach.FrontierIndex - 1);
	}
	else if (Reach.bIsEntry)
	{
		Active.Remove(Reach.FrontierIndex);
	}
	else
	{
		Active.Add(Reach.FrontierIndex - 1);
	}
}

void USpawnerMathComponent::AddToResult(const TArray<int32>& ActiveX, const TArray<int32>& ActiveY,
                                        const TArray<int32>& ActiveZ, TMap<FGridCell, TArray<FTimeWindow>>& Result,
                                        float PreviousInstant, float Time)
{
	FGridCell Cell;
	for (int32 CurrentX : ActiveX)
	{
		for (int32 CurrentY : ActiveY)
		{
			for (int32 CurrentZ : ActiveZ)
			{
				Cell.CellX = CurrentX;
				Cell.CellY = CurrentY;
				Cell.CellZ = CurrentZ;

				FTimeWindow Window;
				Window.InTime = PreviousInstant;
				Window.OutTime = Time;

				TArray<FTimeWindow>& CurrentWindow = Result.FindOrAdd(Cell);
				CurrentWindow.Add(Window);
			}
		}
	}
}

TMap<FGridCell, TArray<FTimeWindow>> USpawnerMathComponent::ConvertCandidateTrajectory(
	TArray<FVector> CheckPoints, float Duration)
{
	TMap<FGridCell, TArray<FTimeWindow>> Result;

	float Distance = 0;
	for (int i = 0; i < CheckPoints.Num() - 1; ++i)
	{
		Distance += (CheckPoints[i + 1] - CheckPoints[i]).Size();
	}

	float Speed = Distance / Duration;
	float DistanceBefore = 0;
	for (int i = 0; i < CheckPoints.Num() - 1; i++)
	{
		float SegmentLength = (CheckPoints[i + 1] - CheckPoints[i]).Size();
		float Pos0X = CheckPoints[i].X;
		float Pos0Y = CheckPoints[i].Y;
		float Pos0Z = CheckPoints[i].Z;

		float Pos1X = CheckPoints[i + 1].X;
		float Pos1Y = CheckPoints[i + 1].Y;
		float Pos1Z = CheckPoints[i + 1].Z;

		TArray<int32> BordersX = CalculateBorder(Pos0X, Pos1X, m_MinPoint.X);
		TArray<int32> BordersY = CalculateBorder(Pos0Y, Pos1Y, m_MinPoint.Y);
		TArray<int32> BordersZ = CalculateBorder(Pos0Z, Pos1Z, m_MinPoint.Z);

		float T0 = DistanceBefore / Speed;
		float T1 = (DistanceBefore + SegmentLength) / Speed;
		float PreviousInstant = T0;

		DistanceBefore += SegmentLength;

		TArray<FBorderReach> AllBordersReach;
		TArray<FBorderReach> BorderReachesX = BordersReachByAxes(Pos0X, Pos1X, m_MinPoint.X, T0, T1, EAxisTypes::X,
		                                                         BordersX);
		for (FBorderReach ReachX : BorderReachesX)
		{
			AllBordersReach.Add(ReachX);
		}

		TArray<FBorderReach> BorderReachesY = BordersReachByAxes(Pos0Y, Pos1Y, m_MinPoint.Y, T0, T1, EAxisTypes::Y,
		                                                         BordersY);
		for (FBorderReach ReachY : BorderReachesY)
		{
			AllBordersReach.Add(ReachY);
		}

		TArray<FBorderReach> BorderReachesZ = BordersReachByAxes(Pos0Z, Pos1Z, m_MinPoint.Z, T0, T1, EAxisTypes::Z,
		                                                         BordersZ);
		for (FBorderReach ReachZ : BorderReachesZ)
		{
			AllBordersReach.Add(ReachZ);
		}

		AllBordersReach.Sort([](const FBorderReach& EntryA, const FBorderReach& EntryB)
		{
			return EntryA.Instant < EntryB.Instant;
		});


		TArray<int32> ActiveX = InitializeActivePoints(Pos0X, m_MinPoint.X);
		TArray<int32> ActiveY = InitializeActivePoints(Pos0Y, m_MinPoint.Y);
		TArray<int32> ActiveZ = InitializeActivePoints(Pos0Z, m_MinPoint.Z);

		for (int k = 0; k < AllBordersReach.Num(); k++)
		{
			AddToResult(ActiveX, ActiveY, ActiveZ, Result, PreviousInstant, AllBordersReach[k].Instant);
			switch (AllBordersReach[k].Axis)
			{
			case EAxisTypes::X:
				{
					ModifyActivePoints(AllBordersReach[k], ActiveX);
					break;
				}
			case EAxisTypes::Y:
				{
					ModifyActivePoints(AllBordersReach[k], ActiveY);
					break;
				}
			case EAxisTypes::Z:
				{
					ModifyActivePoints(AllBordersReach[k], ActiveZ);
					break;
				}
			}

			PreviousInstant = AllBordersReach[k].Instant;
		}
		//Ici on peut faire la boucle parce que les éléments ont déjà été enlevés si nécessaire
		AddToResult(ActiveX, ActiveY, ActiveZ, Result, PreviousInstant, T1);
	}
	return Result;
}

void USpawnerMathComponent::ReverseCandidateTrajectory(TArray<FVector> Checkpoints, float Duration,
                                                       TMap<FGridCell, TArray<FTimeWindow>>& Result)
{
	TArray<FVector> ReverseCheckpoints = Checkpoints;
	Algo::Reverse(ReverseCheckpoints);

	TMap<FGridCell, TArray<FTimeWindow>> ReverseResult = ConvertCandidateTrajectory(ReverseCheckpoints, Duration);

	for (auto& ReverseCells : ReverseResult)
	{
		for (FTimeWindow& Window : ReverseCells.Value)
		{
			Window.InTime += Duration;
			Window.OutTime += Duration;

			TArray<FTimeWindow>& CurrentWindow = Result.FindOrAdd(ReverseCells.Key);
			CurrentWindow.Add(Window);
		}
	}
}

TArray<FTimeWindow> USpawnerMathComponent::MergedTimeRanges(const TMap<FGridCell, TArray<FTimeWindow>>& Result)
{
	TArray<FTimeWindow> ForbiddenTimeRanges;

	for (auto& Cell : Result)
	{
		TArray<FTimeWindow>* CurrentWindow = CellTimeWindows.Find(Cell.Key);
		if (CurrentWindow)
		{
			for (FTimeWindow CandidateWindow : Cell.Value)
			{
				for (FTimeWindow& ActiveWindow : *CurrentWindow)
				{
					float MinTime = ActiveWindow.InTime - CandidateWindow.OutTime;
					float MaxTime = ActiveWindow.OutTime - CandidateWindow.InTime;

					FTimeWindow ForbiddenWindow;
					ForbiddenWindow.InTime = MinTime;
					ForbiddenWindow.OutTime = MaxTime;

					ForbiddenTimeRanges.Add(ForbiddenWindow);
				}
			}
		}
	}

	ForbiddenTimeRanges.Sort([](const FTimeWindow& First, const FTimeWindow& Second)
	{
		return First.InTime < Second.InTime;
	});

	TArray<FTimeWindow> MergesRanges;
	for (int i = 0; i < ForbiddenTimeRanges.Num(); i++)
	{
		if (i == 0)
		{
			MergesRanges.Add(ForbiddenTimeRanges[i]);
		}
		else
		{
			if (ForbiddenTimeRanges[i].InTime <= MergesRanges.Last().OutTime)
			{
				float MinTime = FMath::Min(ForbiddenTimeRanges[i].InTime, MergesRanges.Last().InTime);
				float MaxTime = FMath::Max(ForbiddenTimeRanges[i].OutTime, MergesRanges.Last().OutTime);
				MergesRanges.Last().InTime = MinTime;
				MergesRanges.Last().OutTime = MaxTime;
			}
			else
			{
				MergesRanges.Add(ForbiddenTimeRanges[i]);
			}
		}
	}

	return MergesRanges;
}

TArray<FTimeWindow> USpawnerMathComponent::ComputeForbiddenTimeRanges(TArray<FVector> CheckPoints, float Duration,
                                                                      bool bIsPingPong)
{
	TMap<FGridCell, TArray<FTimeWindow>> Result = ConvertCandidateTrajectory(CheckPoints, Duration);

	if (bIsPingPong)
	{
		ReverseCandidateTrajectory(CheckPoints, Duration, Result);
	}

	TArray<FTimeWindow> MergesRanges = MergedTimeRanges(Result);

	return MergesRanges;
}

TMap<FGridCell, TArray<FTimeWindow>> USpawnerMathComponent::ConvertImmobileCandidate(FVector Position)
{
	TMap<FGridCell, TArray<FTimeWindow>> Result;
	TArray<int32> ActiveX = InitializeActivePoints(Position.X, m_MinPoint.X);
	TArray<int32> ActiveY = InitializeActivePoints(Position.Y, m_MinPoint.Y);
	TArray<int32> ActiveZ = InitializeActivePoints(Position.Z, m_MinPoint.Z);

	AddToResult(ActiveX, ActiveY, ActiveZ, Result, 0.f, TNumericLimits<float>::Max());

	return Result;
}

TArray<FTimeWindow> USpawnerMathComponent::ComputeImmobileForbidden(FVector Position)
{
	TMap<FGridCell, TArray<FTimeWindow>> Result = ConvertImmobileCandidate(Position);
	TArray<FTimeWindow> MergedRanges = MergedTimeRanges(Result);

	return MergedRanges;
}

float USpawnerMathComponent::FindValidSpawnTime(const TArray<FTimeWindow>& TimesRange, bool& bOutIsValid)
{
	bOutIsValid = true;
	float NowTime = GetWorld()->GetTimeSeconds();
	for (int i = 0; i < TimesRange.Num(); i++)
	{
		if (NowTime >= TimesRange[i].InTime && NowTime <= TimesRange[i].OutTime)
		{
			NowTime = TimesRange[i].OutTime;
			if (NowTime >= 100000)
			{
				bOutIsValid = false;
			}
		}
	}

	return NowTime;
}

void USpawnerMathComponent::RegisterActiveObjectTrajectory(AActor* Target)
{
	Target->OnDestroyed.AddDynamic(this, &USpawnerMathComponent::OnActorDestroyed);

	TMap<FGridCell, TArray<FTimeWindow>> Result;
	UInterpToMovementComponent* Component = Target->FindComponentByClass<UInterpToMovementComponent>();

	if (Component)
	{
		// FIX 1 : les PositionControlPoint sont RELATIFS à la position de départ
		// du composant (voir doc Epic : "relative to the root component's initial location").
		// Il faut donc les ramener en coordonnées monde en ajoutant la position de spawn.
		// IMPORTANT : cette fonction doit être appelée AVANT que le composant n'ait tické,
		// sinon GetActorLocation() ne correspondra plus à l'origine utilisée par le composant.
		const FVector StartLocation = Target->GetActorLocation();

		TArray<FVector> Vectors;
		for (const FInterpControlPoint& Point : Component->ControlPoints)
		{
			Vectors.Add(Point.PositionControlPoint + StartLocation);
		}

		// FIX 2 : un seul passage (Duration) ne suffit pas si la cible fait des allers-retours
		// ou boucle indéfiniment - sinon les fenêtres occupées "expirent" alors que la cible bouge encore.
		switch (Component->BehaviourType)
		{
		case EInterpToBehaviourType::OneShot:
		case EInterpToBehaviourType::OneShot_Reverse:
			{
				// La cible s'arrête définitivement après Duration : un seul passage suffit.
				Result = ConvertCandidateTrajectory(Vectors, Component->Duration);
				break;
			}
		case EInterpToBehaviourType::PingPong:
			{
				// Aller-retour qui se répète indéfiniment : on calcule l'aller,
				// on ajoute le retour (comme le fait déjà ComputeForbiddenTimeRanges),
				// puis on répète ce cycle de 2*Duration jusqu'à un horizon raisonnable.
				Result = ConvertCandidateTrajectory(Vectors, Component->Duration);
				ReverseCandidateTrajectory(Vectors, Component->Duration, Result);

				const float CycleLength = Component->Duration * 2.f;
				AppendRepeatingCycles(Result, CycleLength);
				break;
			}
		case EInterpToBehaviourType::Loop_Reset:
		default:
			{
				// Boucle qui recommence au point de départ indéfiniment.
				Result = ConvertCandidateTrajectory(Vectors, Component->Duration);
				AppendRepeatingCycles(Result, Component->Duration);
				break;
			}
		}
	}
	else
	{
		Result = ConvertImmobileCandidate(Target->GetActorLocation());
	}

	for (auto& Cell : Result)
	{
		for (FTimeWindow& Window : Cell.Value)
		{
			Window.InTime += GetWorld()->GetTimeSeconds();
			if (Component && Window.OutTime < TNumericLimits<float>::Max())
			{
				Window.OutTime += GetWorld()->GetTimeSeconds();
			}
			Window.Owner = Target;
			TArray<FTimeWindow>& CurrentWindow = CellTimeWindows.FindOrAdd(Cell.Key);
			CurrentWindow.Add(Window);
		}
		TArray<FGridCell>& Cells = ObjectCellList.FindOrAdd(Target);
		Cells.Add(Cell.Key);
	}
}

void USpawnerMathComponent::AppendRepeatingCycles(TMap<FGridCell, TArray<FTimeWindow>>& Result, float CycleLength)
{
	if (CycleLength <= 0.f)
	{
		return;
	}

	constexpr float Horizon = 600.f; // 10 minutes d'anticipation, à ajuster selon vos besoins
	const int32 NumExtraCycles = FMath::FloorToInt(Horizon / CycleLength);

	TArray<TPair<FGridCell, FTimeWindow>> BaseWindows;
	for (const auto& Cell : Result)
	{
		for (const FTimeWindow& Window : Cell.Value)
		{
			BaseWindows.Add(TPair<FGridCell, FTimeWindow>(Cell.Key, Window));
		}
	}

	for (int32 CycleIndex = 1; CycleIndex <= NumExtraCycles; CycleIndex++)
	{
		const float Offset = CycleIndex * CycleLength;
		for (const auto& Pair : BaseWindows)
		{
			FTimeWindow ShiftedWindow;
			ShiftedWindow.InTime = Pair.Value.InTime + Offset;
			ShiftedWindow.OutTime = Pair.Value.OutTime + Offset;

			TArray<FTimeWindow>& CurrentWindow = Result.FindOrAdd(Pair.Key);
			CurrentWindow.Add(ShiftedWindow);
		}
	}
}

void USpawnerMathComponent::OnActorDestroyed(AActor* Actor)
{
	TArray<FGridCell>* FindCells = ObjectCellList.Find(Actor);
	if (FindCells)
	{
		for (FGridCell Cell : *FindCells)
		{
			TArray<FTimeWindow>* CurrentWindow = CellTimeWindows.Find(Cell);
			if (CurrentWindow)
			{
				CurrentWindow->RemoveAll([Actor](const FTimeWindow& Window)
				{
					return Window.Owner == Actor;
				});	
			}
		}
	
		ObjectCellList.Remove(Actor);	
	}
}
