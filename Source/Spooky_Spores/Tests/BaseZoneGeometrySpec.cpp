// Automation Spec for the pure geometry of the base zone (no UWorld, no actor).
// Source of truth: docs/specs/base-zone.md, criteria A1-A7, A9, A10 (static part), R1-R6, D1-D2.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BaseZoneTestHelpers.h"
#include "Base/BaseZone.h"
#include <limits>

BEGIN_DEFINE_SPEC(FBaseZoneGeometrySpec, "SpookySpores.Base.Zone.Geometry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	void ExpectPoint(const TCHAR* Label, const FTransform& Transform, const FVector& Extent,
	                 const FVector& WorldPoint, bool bIgnoreHeight, bool bExpectedInside);

END_DEFINE_SPEC(FBaseZoneGeometrySpec)

void FBaseZoneGeometrySpec::ExpectPoint(const TCHAR* Label, const FTransform& Transform, const FVector& Extent,
                                        const FVector& WorldPoint, bool bIgnoreHeight, bool bExpectedInside)
{
	const bool bActual = ABaseZone::IsPointInOrientedBox(Transform, Extent, WorldPoint, bIgnoreHeight);
	TestTrue(FString::Printf(TEXT("%s: point %s (ignoreHeight=%d) expected %s"), Label, *WorldPoint.ToString(),
	                         bIgnoreHeight ? 1 : 0, bExpectedInside ? TEXT("inside") : TEXT("outside")),
	         bActual == bExpectedInside);
}

void FBaseZoneGeometrySpec::Define()
{
	using namespace BaseZoneTest;

	Describe("Membership (IsPointInOrientedBox)", [this]()
	{
		It("A1: axis-aligned box, extent is a half-size on every axis", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const FVector E(500.0, 300.0, 100.0);
			ExpectPoint(TEXT("A1"), T, E, FVector(0, 0, 0), false, true);
			ExpectPoint(TEXT("A1"), T, E, FVector(499, 299, 99), false, true);
			ExpectPoint(TEXT("A1"), T, E, FVector(-499, -299, -99), false, true);
			ExpectPoint(TEXT("A1"), T, E, FVector(501, 0, 0), false, false);
			ExpectPoint(TEXT("A1"), T, E, FVector(0, 301, 0), false, false);
			ExpectPoint(TEXT("A1"), T, E, FVector(0, 0, 101), false, false);
			ExpectPoint(TEXT("A1"), T, E, FVector(-501, 0, 0), false, false);
		});

		It("A2: borders are inclusive, 0.5 cm outside is outside (on every face)", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const FVector E(500.0, 300.0, 100.0);
			ExpectPoint(TEXT("A2"), T, E, FVector(500, 0, 0), false, true);
			ExpectPoint(TEXT("A2"), T, E, FVector(-500, 0, 0), false, true);
			ExpectPoint(TEXT("A2"), T, E, FVector(0, 300, 0), false, true);
			ExpectPoint(TEXT("A2"), T, E, FVector(0, 0, -100), false, true);
			ExpectPoint(TEXT("A2"), T, E, FVector(500.5, 0, 0), false, false);
			ExpectPoint(TEXT("A2"), T, E, FVector(-500.5, 0, 0), false, false);
			ExpectPoint(TEXT("A2"), T, E, FVector(0, 300.5, 0), false, false);
			ExpectPoint(TEXT("A2"), T, E, FVector(0, 0, 100.5), false, false);
		});

		It("A3: box rotated 90 degrees in yaw (the rotation must be applied)", [this]()
		{
			const FTransform T = MakeTransform(FVector(1000, 0, 0), FRotator(0, 90, 0));
			const FVector E(500.0, 200.0, 100.0);
			ExpectPoint(TEXT("A3"), T, E, FVector(1000, 400, 0), false, true);
			ExpectPoint(TEXT("A3"), T, E, FVector(1400, 0, 0), false, false);
			// Both faces of the rotated short axis (world X) and of the long axis (world Y).
			ExpectPoint(TEXT("A3"), T, E, FVector(1199, 0, 0), false, true);
			ExpectPoint(TEXT("A3"), T, E, FVector(1201, 0, 0), false, false);
			ExpectPoint(TEXT("A3"), T, E, FVector(1000, -499, 0), false, true);
			ExpectPoint(TEXT("A3"), T, E, FVector(1000, 501, 0), false, false);
		});

		It("A4: box rotated 45 degrees in yaw", [this]()
		{
			const FTransform T = MakeTransform(FVector(1000, 0, 0), FRotator(0, 45, 0));
			const FVector E(500.0, 200.0, 100.0);
			const FVector Center(1000, 0, 0);
			const FVector Dir(FMath::Cos(UE_DOUBLE_PI / 4.0), FMath::Sin(UE_DOUBLE_PI / 4.0), 0.0);
			ExpectPoint(TEXT("A4"), T, E, Center + Dir * 400.0, false, true);
			ExpectPoint(TEXT("A4"), T, E, Center + Dir * 600.0, false, false);
			ExpectPoint(TEXT("A4"), T, E, Center + FVector(450, 0, 0), false, false);
		});

		It("A5: box tilted 30 degrees in pitch (local space in 3D)", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector, FRotator(30, 0, 0));
			const FVector E(500.0, 300.0, 100.0);
			ExpectPoint(TEXT("A5 local z=90"), T, E, T.TransformPosition(FVector(0, 0, 90)), false, true);
			ExpectPoint(TEXT("A5 local z=110"), T, E, T.TransformPosition(FVector(0, 0, 110)), false, false);
			// Inside the world-aligned box (|z|=0) but outside the tilted one (local z = +-225): a world-space implementation says inside.
			ExpectPoint(TEXT("A5 world-aligned trap"), T, E, FVector(450, 0, 0), false, false);
		});

		It("A6: non-uniform scale is applied exactly once", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, FVector(3, 1, 1));
			const FVector E(100.0, 100.0, 100.0);
			ExpectPoint(TEXT("A6"), T, E, FVector(250, 0, 0), false, true);
			ExpectPoint(TEXT("A6"), T, E, FVector(299, 0, 0), false, true);
			ExpectPoint(TEXT("A6"), T, E, FVector(301, 0, 0), false, false);
			ExpectPoint(TEXT("A6"), T, E, FVector(0, 250, 0), false, false);
			ExpectPoint(TEXT("A6"), T, E, FVector(350, 0, 0), false, false);
		});

		It("A7: bIgnoreHeight skips local Z only, never X/Y", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const FVector E(500.0, 300.0, 100.0);
			ExpectPoint(TEXT("A7"), T, E, FVector(0, 0, 5000), true, true);
			ExpectPoint(TEXT("A7"), T, E, FVector(0, 0, 5000), false, false);
			ExpectPoint(TEXT("A7"), T, E, FVector(501, 0, 5000), true, false);
			ExpectPoint(TEXT("A7"), T, E, FVector(501, 0, 5000), false, false);
			ExpectPoint(TEXT("A7"), T, E, FVector(0, 301, -5000), true, false);
		});

		It("A7 (tilted): bIgnoreHeight ignores the LOCAL Z axis, not the world Z", [this]()
		{
			// Pitch 90: local Z points along world X. A point far along world X is "high" locally and must be ignored,
			// while a point far along world Z is "wide" in local X and must be rejected.
			const FTransform T = MakeTransform(FVector::ZeroVector, FRotator(90, 0, 0));
			const FVector E(500.0, 300.0, 100.0);
			ExpectPoint(TEXT("A7 tilted local-high"), T, E, FVector(5000, 0, 0), true, true);
			ExpectPoint(TEXT("A7 tilted local-wide"), T, E, FVector(0, 0, 5000), true, false);
		});

		It("A10: negative scale (mirror) keeps the zone valid and symmetric", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, FVector(-3, 1, 1));
			const FVector E(100.0, 100.0, 100.0);
			TestTrue(TEXT("A10: negative scale is a valid zone"), ABaseZone::IsOrientedBoxValid(T, E));
			ExpectPoint(TEXT("A10"), T, E, FVector(250, 0, 0), false, true);
			ExpectPoint(TEXT("A10"), T, E, FVector(-250, 0, 0), false, true);
			ExpectPoint(TEXT("A10"), T, E, FVector(350, 0, 0), false, false);
			ExpectPoint(TEXT("A10"), T, E, FVector(-350, 0, 0), false, false);
			ExpectPoint(TEXT("A10"), T, E, FVector(0, 250, 0), false, false);
		});

		It("A10: negative scale still yields random points inside the mirrored box", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, FVector(-3, 1, 1));
			const FVector E(100.0, 100.0, 100.0);
			TArray<FVector> Points;
			TestTrue(TEXT("A10: draws succeed with a negative scale"), DrawPoints(T, E, false, 500, Points));
			for (const FVector& P : Points)
			{
				if (FMath::Abs(P.X) > 300.0 + IndependentTolerance || FMath::Abs(P.Y) > 100.0 + IndependentTolerance
					|| FMath::Abs(P.Z) > 100.0 + IndependentTolerance)
				{
					AddError(FString::Printf(TEXT("A10: drawn point %s is outside the mirrored box"), *P.ToString()));
					break;
				}
			}
		});

		It("A9: non-finite points are rejected in both height modes", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const FVector E(500.0, 300.0, 100.0);
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();

			// Control: a finite point is accepted, so the "false" below is caused by the non-finite value.
			ExpectPoint(TEXT("A9 control"), T, E, FVector(1, 2, 3), false, true);
			ExpectPoint(TEXT("A9 control"), T, E, FVector(1, 2, 3), true, true);

			for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
			{
				const bool bIgnoreHeight = (HeightMode == 1);
				ExpectPoint(TEXT("A9 NaN x"), T, E, FVector(NaN, 0, 0), bIgnoreHeight, false);
				ExpectPoint(TEXT("A9 NaN y"), T, E, FVector(0, NaN, 0), bIgnoreHeight, false);
				ExpectPoint(TEXT("A9 NaN z"), T, E, FVector(0, 0, NaN), bIgnoreHeight, false);
				ExpectPoint(TEXT("A9 +inf x"), T, E, FVector(Inf, 0, 0), bIgnoreHeight, false);
				ExpectPoint(TEXT("A9 -inf z"), T, E, FVector(0, 0, -Inf), bIgnoreHeight, false);
			}
		});
	});

	Describe("Validity and degenerate zones (D1, D2)", [this]()
	{
		It("D1: zero or negative extent on one axis makes the zone invalid and rejects every point, center included", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const TArray<FVector> BadExtents = { FVector(0, 300, 100), FVector(500, 0, 100), FVector(500, 300, 0), FVector(-100, 300, 100) };
			for (const FVector& E : BadExtents)
			{
				TestFalse(FString::Printf(TEXT("D1: extent %s is invalid"), *E.ToString()), ABaseZone::IsOrientedBoxValid(T, E));
				for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
				{
					const bool bIgnoreHeight = (HeightMode == 1);
					ExpectPoint(TEXT("D1 center of degenerate box"), T, E, FVector::ZeroVector, bIgnoreHeight, false);
					FVector Out(1, 2, 3);
					TestFalse(FString::Printf(TEXT("D1/D2: random draw on extent %s returns false (ignoreHeight=%d)"), *E.ToString(), HeightMode),
					          ABaseZone::TryGetRandomPointInOrientedBox(T, E, bIgnoreHeight, Out));
				}
			}
		});

		It("D1: zero scale on one axis makes the zone invalid (the center must NOT count as inside)", [this]()
		{
			const FVector E(100.0, 100.0, 100.0);
			const TArray<FVector> BadScales = { FVector(0, 1, 1), FVector(1, 0, 1), FVector(1, 1, 0) };
			for (const FVector& S : BadScales)
			{
				const FTransform T = MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, S);
				TestFalse(FString::Printf(TEXT("D1: scale %s is invalid"), *S.ToString()), ABaseZone::IsOrientedBoxValid(T, E));
				ExpectPoint(TEXT("D1 center with zero scale"), T, E, FVector::ZeroVector, false, false);
				ExpectPoint(TEXT("D1 center with zero scale"), T, E, FVector::ZeroVector, true, false);
				FVector Out = FVector::ZeroVector;
				TestFalse(TEXT("D1/D2: random draw with a zero scale returns false"),
				          ABaseZone::TryGetRandomPointInOrientedBox(T, E, false, Out));
			}
		});

		It("D1: validity threshold is strictly greater than KINDA_SMALL_NUMBER (spec section 4)", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			TestFalse(TEXT("extent == KINDA_SMALL_NUMBER is invalid"),
			          ABaseZone::IsOrientedBoxValid(T, FVector(UE_KINDA_SMALL_NUMBER, 100, 100)));
			TestTrue(TEXT("extent 2*KINDA_SMALL_NUMBER is valid"),
			         ABaseZone::IsOrientedBoxValid(T, FVector(2.0 * UE_KINDA_SMALL_NUMBER, 100, 100)));
			TestTrue(TEXT("control: ordinary extent is valid"), ABaseZone::IsOrientedBoxValid(T, FVector(500, 300, 100)));
		});
	});

	Describe("Non-finite zone inputs (validity must be robust to NaN / infinity)", [this]()
	{
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		const double Inf = std::numeric_limits<double>::infinity();

		It("NaN transform: IsPointInOrientedBox is false (Abs(NaN) > x is false on every axis, so a naive test says inside)", [this, NaN]()
		{
			const FVector E(500.0, 300.0, 100.0);
			const FVector Probe(1, 2, 3);

			// Control: the same probe with a finite transform is inside, so the "false" below is caused by the NaN.
			ExpectPoint(TEXT("NaN control"), MakeTransform(FVector::ZeroVector), E, Probe, false, true);
			ExpectPoint(TEXT("NaN control"), MakeTransform(FVector::ZeroVector), E, Probe, true, true);

			const FTransform NaNRotation(FQuat(NaN, NaN, NaN, NaN), FVector::ZeroVector, FVector::OneVector);
			const FTransform NaNTranslation(FQuat::Identity, FVector(NaN, 0, 0), FVector::OneVector);
			const FTransform NaNTranslationZ(FQuat::Identity, FVector(0, 0, NaN), FVector::OneVector);

			for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
			{
				const bool bIgnoreHeight = (HeightMode == 1);
				ExpectPoint(TEXT("NaN rotation"), NaNRotation, E, Probe, bIgnoreHeight, false);
				ExpectPoint(TEXT("NaN translation X"), NaNTranslation, E, Probe, bIgnoreHeight, false);
				ExpectPoint(TEXT("NaN translation Z"), NaNTranslationZ, E, Probe, bIgnoreHeight, false);
			}
		});

		It("Infinite or NaN extent: IsOrientedBoxValid is false on every axis (+inf, -inf, NaN)", [this, NaN, Inf]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			TestTrue(TEXT("control: finite extent is valid"), ABaseZone::IsOrientedBoxValid(T, FVector(500, 300, 100)));

			const double Bad[] = { Inf, -Inf, NaN };
			for (const double Value : Bad)
			{
				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					FVector E(500, 300, 100);
					E[Axis] = Value;
					TestFalse(FString::Printf(TEXT("extent axis %d = %f is invalid"), Axis, Value), ABaseZone::IsOrientedBoxValid(T, E));
				}
			}
		});

		It("Infinite or NaN scale: IsOrientedBoxValid is false on every axis (+inf, -inf, NaN)", [this, NaN, Inf]()
		{
			const FVector E(100, 100, 100);
			const double Bad[] = { Inf, -Inf, NaN };
			for (const double Value : Bad)
			{
				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					FVector S(1, 1, 1);
					S[Axis] = Value;
					const FTransform T = MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, S);
					TestFalse(FString::Printf(TEXT("scale axis %d = %f is invalid"), Axis, Value), ABaseZone::IsOrientedBoxValid(T, E));
				}
			}
		});

		It("R1: a random draw on an infinite extent or scale returns false, and never true with a non-finite point", [this, NaN, Inf]()
		{
			TArray<TPair<FTransform, FVector>> Cases;
			Cases.Add({ MakeTransform(FVector::ZeroVector), FVector(Inf, 300, 100) });
			Cases.Add({ MakeTransform(FVector::ZeroVector), FVector(500, Inf, 100) });
			Cases.Add({ MakeTransform(FVector::ZeroVector), FVector(500, 300, Inf) });
			Cases.Add({ MakeTransform(FVector::ZeroVector), FVector(NaN, 300, 100) });
			Cases.Add({ MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, FVector(Inf, 1, 1)), FVector(100, 100, 100) });
			Cases.Add({ MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, FVector(1, 1, NaN)), FVector(100, 100, 100) });

			// Non-finite TRANSFORM with a finite extent and scale: IsPointInZone rejects these, so a successful draw
			// would break R1 ("every drawn point satisfies IsPointInZone").
			const FVector FiniteExtent(100, 100, 100);
			Cases.Add({ FTransform(FQuat::Identity, FVector(NaN, 0, 0), FVector::OneVector), FiniteExtent });
			Cases.Add({ FTransform(FQuat::Identity, FVector(0, 0, NaN), FVector::OneVector), FiniteExtent });
			Cases.Add({ FTransform(FQuat::Identity, FVector(Inf, 0, 0), FVector::OneVector), FiniteExtent });
			Cases.Add({ FTransform(FQuat(NaN, NaN, NaN, NaN), FVector::ZeroVector, FVector::OneVector), FiniteExtent });

			// Control: the same finite extent with a finite transform succeeds, so the "false" cases are caused by the non-finite input.
			for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
			{
				FVector ControlOut = FVector::ZeroVector;
				TestTrue(FString::Printf(TEXT("control (ignoreHeight=%d): a finite transform draws successfully"), HeightMode),
					ABaseZone::TryGetRandomPointInOrientedBox(MakeTransform(FVector::ZeroVector), FiniteExtent, HeightMode == 1, ControlOut));
				TestFalse(TEXT("control: the drawn point is finite"), ControlOut.ContainsNaN());
			}

			for (int32 CaseIndex = 0; CaseIndex < Cases.Num(); ++CaseIndex)
			{
				for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
				{
					const bool bIgnoreHeight = (HeightMode == 1);
					FVector Out(1, 2, 3);
					const bool bResult = ABaseZone::TryGetRandomPointInOrientedBox(Cases[CaseIndex].Key, Cases[CaseIndex].Value, bIgnoreHeight, Out);
					TestFalse(FString::Printf(TEXT("case %d (ignoreHeight=%d): the draw returns false"), CaseIndex, HeightMode), bResult);
					if (bResult)
					{
						// Invariant R1: a successful draw must be a finite member of the zone.
						TestFalse(FString::Printf(TEXT("case %d: a true result must never carry a non-finite point"), CaseIndex), Out.ContainsNaN());
					}

					// Consistency: where the draw refuses, the membership test must refuse too (the center is inside any finite box).
					if (CaseIndex >= 6)
					{
						TestFalse(FString::Printf(TEXT("case %d (ignoreHeight=%d): IsPointInOrientedBox is false for the same transform"), CaseIndex, HeightMode),
							ABaseZone::IsPointInOrientedBox(Cases[CaseIndex].Key, Cases[CaseIndex].Value, FVector::ZeroVector, bIgnoreHeight));
					}
				}
			}
		});
	});

	Describe("Random draw (TryGetRandomPointInOrientedBox)", [this]()
	{
		It("R1: every drawn point is a member of the zone, for every reference config and both height modes", [this]()
		{
			for (const FBoxConfig& Config : MakeReferenceConfigs())
			{
				for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
				{
					const bool bIgnoreHeight = (HeightMode == 1);
					TArray<FVector> Points;
					const FString Label = FString::Printf(TEXT("R1 %s ignoreHeight=%d"), *Config.Name, HeightMode);
					if (!TestTrue(*(Label + TEXT(": all draws succeed")), DrawPoints(Config.Transform, Config.Extent, bIgnoreHeight, DrawCount, Points)))
					{
						continue;
					}

					int32 NotMembers = 0;
					int32 OutsideIndependentCheck = 0;
					for (const FVector& P : Points)
					{
						if (!ABaseZone::IsPointInOrientedBox(Config.Transform, Config.Extent, P, bIgnoreHeight))
						{
							++NotMembers;
						}
						// Independent re-check with the engine's own inverse transform (does not rely on the implementation under test).
						const FVector Local = Config.Transform.InverseTransformPosition(P);
						if (FMath::Abs(Local.X) > Config.Extent.X + IndependentTolerance
							|| FMath::Abs(Local.Y) > Config.Extent.Y + IndependentTolerance
							|| FMath::Abs(Local.Z) > Config.Extent.Z + IndependentTolerance)
						{
							++OutsideIndependentCheck;
						}
					}
					TestEqual(*(Label + TEXT(": points rejected by IsPointInOrientedBox")), NotMembers, 0);
					TestEqual(*(Label + TEXT(": points outside the local box (independent check)")), OutsideIndependentCheck, 0);
				}
			}
		});

		It("R2: uniform coverage on every local axis (all deciles hit, mean near the center)", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const FVector E(500.0, 300.0, 100.0);
			TArray<FVector> Points;
			if (!TestTrue(TEXT("R2: draws succeed"), DrawPoints(T, E, false, DrawCount, Points)))
			{
				return;
			}

			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const double Half = E[Axis];
				int32 Bins[10] = {};
				double Sum = 0.0;
				for (const FVector& P : Points)
				{
					const FVector Local = T.InverseTransformPosition(P);
					const double L = Local[Axis];
					Sum += L;
					const int32 Bin = FMath::Clamp(FMath::FloorToInt32((L + Half) / (2.0 * Half) * 10.0), 0, 9);
					++Bins[Bin];
				}
				for (int32 Bin = 0; Bin < 10; ++Bin)
				{
					TestTrue(FString::Printf(TEXT("R2: axis %d decile %d is not empty"), Axis, Bin), Bins[Bin] > 0);
				}
				const double Mean = Sum / Points.Num();
				TestTrue(FString::Printf(TEXT("R2: axis %d mean %.2f within 10%% of the half-extent"), Axis, Mean),
				         FMath::Abs(Mean) < 0.1 * Half);
			}
		});

		It("R3: the rotation is applied to the draw (yaw 90 box)", [this]()
		{
			const FTransform T = MakeTransform(FVector(1000, 0, 0), FRotator(0, 90, 0));
			const FVector E(500.0, 200.0, 100.0);
			TArray<FVector> Points;
			if (!TestTrue(TEXT("R3: draws succeed"), DrawPoints(T, E, false, DrawCount, Points)))
			{
				return;
			}
			double MinX = TNumericLimits<double>::Max(), MaxX = TNumericLimits<double>::Lowest();
			double MinY = TNumericLimits<double>::Max(), MaxY = TNumericLimits<double>::Lowest();
			for (const FVector& P : Points)
			{
				MinX = FMath::Min(MinX, P.X); MaxX = FMath::Max(MaxX, P.X);
				MinY = FMath::Min(MinY, P.Y); MaxY = FMath::Max(MaxY, P.Y);
			}
			TestTrue(TEXT("R3: world X stays inside [800, 1200]"), MinX >= 800.0 - IndependentTolerance && MaxX <= 1200.0 + IndependentTolerance);
			TestTrue(TEXT("R3: world X range covers more than 80% of 400"), (MaxX - MinX) > 0.8 * 400.0);
			TestTrue(TEXT("R3: world Y stays inside [-500, 500]"), MinY >= -500.0 - IndependentTolerance && MaxY <= 500.0 + IndependentTolerance);
			TestTrue(TEXT("R3: world Y range covers more than 80% of 1000"), (MaxY - MinY) > 0.8 * 1000.0);
		});

		It("R4: non-uniform scale (3,1,1) stretches the draw on world X only", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, FVector(3, 1, 1));
			const FVector E(100.0, 100.0, 100.0);
			TArray<FVector> Points;
			if (!TestTrue(TEXT("R4: draws succeed"), DrawPoints(T, E, false, DrawCount, Points)))
			{
				return;
			}
			double MinX = TNumericLimits<double>::Max(), MaxX = TNumericLimits<double>::Lowest();
			double MaxAbsY = 0.0;
			for (const FVector& P : Points)
			{
				MinX = FMath::Min(MinX, P.X); MaxX = FMath::Max(MaxX, P.X);
				MaxAbsY = FMath::Max(MaxAbsY, FMath::Abs(P.Y));
			}
			TestTrue(TEXT("R4: world X reaches beyond +270"), MaxX > 270.0);
			TestTrue(TEXT("R4: world X reaches beyond -270"), MinX < -270.0);
			TestTrue(TEXT("R4: world X stays inside [-300, 300]"), MaxX <= 300.0 + IndependentTolerance && MinX >= -300.0 - IndependentTolerance);
			TestTrue(TEXT("R4: world Y stays inside [-100, 100]"), MaxAbsY <= 100.0 + IndependentTolerance);
		});

		It("R5: bIgnoreHeight=true draws on the mid plane (local Z = 0), also for a tilted box", [this]()
		{
			for (const FBoxConfig& Config : MakeReferenceConfigs())
			{
				TArray<FVector> Points;
				if (!TestTrue(*FString::Printf(TEXT("R5 %s: draws succeed"), *Config.Name), DrawPoints(Config.Transform, Config.Extent, true, DrawCount, Points)))
				{
					continue;
				}
				double MaxAbsLocalZ = 0.0;
				for (const FVector& P : Points)
				{
					MaxAbsLocalZ = FMath::Max(MaxAbsLocalZ, FMath::Abs(Config.Transform.InverseTransformPosition(P).Z));
				}
				TestTrue(*FString::Printf(TEXT("R5 %s: max |local Z| = %f <= 0.01"), *Config.Name, MaxAbsLocalZ), MaxAbsLocalZ <= 0.01);
			}
		});

		It("R5: for an untilted box, world Z equals the center Z when height is ignored", [this]()
		{
			const FTransform T = MakeTransform(FVector(100, -200, 250));
			const FVector E(500.0, 300.0, 100.0);
			TArray<FVector> Points;
			if (!TestTrue(TEXT("R5: draws succeed"), DrawPoints(T, E, true, 500, Points)))
			{
				return;
			}
			for (const FVector& P : Points)
			{
				if (!FMath::IsNearlyEqual(P.Z, 250.0, 0.01))
				{
					AddError(FString::Printf(TEXT("R5: world Z %f differs from the center Z 250"), P.Z));
					break;
				}
			}
		});

		It("R5: bIgnoreHeight=false covers at least 80% of the local Z range", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const FVector E(500.0, 300.0, 100.0);
			TArray<FVector> Points;
			if (!TestTrue(TEXT("R5: draws succeed"), DrawPoints(T, E, false, DrawCount, Points)))
			{
				return;
			}
			double MinZ = TNumericLimits<double>::Max(), MaxZ = TNumericLimits<double>::Lowest();
			for (const FVector& P : Points)
			{
				const double LocalZ = T.InverseTransformPosition(P).Z;
				MinZ = FMath::Min(MinZ, LocalZ);
				MaxZ = FMath::Max(MaxZ, LocalZ);
			}
			TestTrue(TEXT("R5: local Z span > 80% of [-Ez, Ez]"), (MaxZ - MinZ) > 0.8 * 2.0 * E.Z);
		});

		It("R6: draws are not frozen on a seed (consecutive and successive series differ)", [this]()
		{
			const FTransform T = MakeTransform(FVector::ZeroVector);
			const FVector E(500.0, 300.0, 100.0);
			TArray<FVector> SeriesA, SeriesB;
			if (!TestTrue(TEXT("R6: first series succeeds"), DrawPoints(T, E, false, 100, SeriesA))
				|| !TestTrue(TEXT("R6: second series succeeds"), DrawPoints(T, E, false, 100, SeriesB)))
			{
				return;
			}
			TestTrue(TEXT("R6: the two series are not identical"), SeriesA != SeriesB);

			int32 DistinctInSeries = 0;
			for (int32 Index = 1; Index < SeriesA.Num(); ++Index)
			{
				if (!SeriesA[Index].Equals(SeriesA[0], 0.0))
				{
					++DistinctInSeries;
				}
			}
			TestTrue(TEXT("R6: consecutive draws within a series are not all the same point"), DistinctInSeries > 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
