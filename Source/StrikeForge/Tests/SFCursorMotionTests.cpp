#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/SFCursorMotion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCursorMotionTest, "StrikeForge.Input.CursorSensitivity.NoIdleDrift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCursorMotionTest::RunTest(const FString& Parameters)
{
	for (float Sensitivity : {.1f, .65f, 1.f, 1.7f, 3.f})
	{
		FSFCursorMotion Motion;
		FVector2D Actual(-500., 300.); // A second monitor can have negative desktop coordinates.
		TestEqual(TEXT("First sample only establishes a baseline"), Motion.Adjust(Actual, Sensitivity), Actual);
		Actual = Motion.Adjust(Actual + FVector2D(13., -7.), Sensitivity);
		// Simulate OS rounding/clipping differing from the requested position.
		Actual.X += 1.;
		Motion.RecordActualPosition(Actual);
		for (int32 Frame = 0; Frame < 600; ++Frame)
			if (Motion.Adjust(Actual, Sensitivity) != Actual)
			{
				AddError(FString::Printf(TEXT("Idle cursor moved at sensitivity %.2f, frame %d"), Sensitivity, Frame));
				return false;
			}
		Motion.Reset(); // Focus, viewport geometry or sensitivity changed.
		Actual = FVector2D(1500., 800.);
		TestEqual(TEXT("Re-entry never scales the jump between windows"), Motion.Adjust(Actual, Sensitivity), Actual);
	}
	FSFCursorMotion Slow;
	FVector2D Actual(100., 100.);
	Slow.Adjust(Actual, .1f);
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Actual = Slow.Adjust(Actual + FVector2D(1., 0.), .1f);
		Slow.RecordActualPosition(Actual);
	}
	TestEqual(TEXT("Slow motion retains fractional input"), Actual, FVector2D(101., 100.));
	FSFCursorMotion Fast;
	Fast.Adjust(FVector2D(100., 100.), 3.f);
	TestEqual(TEXT("Actual mouse input still uses sensitivity"),
		Fast.Adjust(FVector2D(102., 99.), 3.f), FVector2D(106., 97.));
	return true;
}
#endif
