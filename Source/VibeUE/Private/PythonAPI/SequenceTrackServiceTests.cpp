// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/USequenceTrackService.h"
#include "AIServiceTestFixture.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneMaterialTrack.h"

static const EAutomationTestFlags kSequenceTrackTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

using VibeAITest::FScopedFixtureReset;

namespace
{
	const TCHAR* SequenceTestPath = TEXT("/Game/Developers/VibeUESequenceTests/LS_VibeSequenceTrackTest");

	/**
	 * A Level Sequence with two bindings, "Cloud" and "Other", each with a component material track:
	 * Cloud animates Cloud_GlobalCoverage (2 keys), Cloud_GlobalDensity (1 key) and the color Tint (1 key);
	 * Other animates Cloud_GlobalCoverage (1 key).
	 */
	ULevelSequence* MakeFixtureSequence()
	{
		UPackage* Package = CreatePackage(SequenceTestPath);
		ULevelSequence* Sequence = NewObject<ULevelSequence>(Package, FName(*FPackageName::GetShortName(SequenceTestPath)),
			RF_Public | RF_Standalone | RF_Transactional);
		Sequence->Initialize();
		FAssetRegistryModule::AssetCreated(Sequence);

		UMovieScene* MovieScene = Sequence->GetMovieScene();
		const FFrameNumber Start(0), End(MovieScene->GetTickResolution().AsFrameNumber(1.0));

		const FGuid Cloud = MovieScene->AddPossessable(TEXT("Cloud"), UStaticMeshComponent::StaticClass());
		UMovieSceneComponentMaterialTrack* CloudTrack = MovieScene->AddTrack<UMovieSceneComponentMaterialTrack>(Cloud);
		CloudTrack->AddScalarParameterKey(TEXT("Cloud_GlobalCoverage"), Start, -1.0f);
		CloudTrack->AddScalarParameterKey(TEXT("Cloud_GlobalCoverage"), End, -0.1f);
		CloudTrack->AddScalarParameterKey(TEXT("Cloud_GlobalDensity"), Start, 0.5f);
		CloudTrack->AddColorParameterKey(TEXT("Tint"), Start, FLinearColor::Red);

		const FGuid Other = MovieScene->AddPossessable(TEXT("Other"), UStaticMeshComponent::StaticClass());
		UMovieSceneComponentMaterialTrack* OtherTrack = MovieScene->AddTrack<UMovieSceneComponentMaterialTrack>(Other);
		OtherTrack->AddScalarParameterKey(TEXT("Cloud_GlobalCoverage"), Start, -1.0f);
		return Sequence;
	}

	const FSequenceMaterialParameterInfo* FindParameter(const TArray<FSequenceMaterialParameterInfo>& Infos,
		const FString& Binding, const FString& Parameter)
	{
		return Infos.FindByPredicate([&](const FSequenceMaterialParameterInfo& Info)
		{
			return Info.BindingName == Binding && Info.ParameterName == Parameter;
		});
	}
}

// list_material_parameters reports each animated parameter with its binding, type and keys.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSequenceTrackListTest,
	"VibeUE.SequenceTrack.ListMaterialParameters", kSequenceTrackTestFlags)
bool FVibeSequenceTrackListTest::RunTest(const FString&)
{
	FScopedFixtureReset Reset(SequenceTestPath);
	MakeFixtureSequence();

	const TArray<FSequenceMaterialParameterInfo> Infos = USequenceTrackService::ListMaterialParameters(SequenceTestPath);
	TestEqual(TEXT("four parameters across the two bindings"), Infos.Num(), 4);

	const FSequenceMaterialParameterInfo* Coverage = FindParameter(Infos, TEXT("Cloud"), TEXT("Cloud_GlobalCoverage"));
	if (TestNotNull(TEXT("Cloud's Cloud_GlobalCoverage is listed"), Coverage))
	{
		TestEqual(TEXT("its type is Scalar"), Coverage->ParameterType, FString(TEXT("Scalar")));
		TestEqual(TEXT("it has two keys"), Coverage->KeyCount, 2);
		TestEqual(TEXT("its track class"), Coverage->TrackClass, FString(TEXT("MovieSceneComponentMaterialTrack")));
		TestEqual(TEXT("it is in the track's first section"), Coverage->SectionIndex, 0);
	}
	const FSequenceMaterialParameterInfo* Tint = FindParameter(Infos, TEXT("Cloud"), TEXT("Tint"));
	if (TestNotNull(TEXT("Cloud's Tint is listed"), Tint))
	{
		TestEqual(TEXT("its type is Color"), Tint->ParameterType, FString(TEXT("Color")));
		TestEqual(TEXT("it has one key"), Tint->KeyCount, 1);
	}
	TestNotNull(TEXT("Other's Cloud_GlobalCoverage is listed"), FindParameter(Infos, TEXT("Other"), TEXT("Cloud_GlobalCoverage")));
	TestEqual(TEXT("a missing asset lists nothing"),
		USequenceTrackService::ListMaterialParameters(TEXT("/Game/Developers/VibeUESequenceTests/LS_DoesNotExist")).Num(), 0);
	return true;
}

// remove_material_parameter removes the parameter and its curve from the named binding only, leaves everything else,
// refuses a parameter that is not there without changing the sequence, and is one undoable transaction.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSequenceTrackRemoveTest,
	"VibeUE.SequenceTrack.RemoveMaterialParameter", kSequenceTrackTestFlags)
bool FVibeSequenceTrackRemoveTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Needs the editor with no PIE session running."));
		return false;
	}
	FScopedFixtureReset Reset(SequenceTestPath);
	ULevelSequence* Sequence = MakeFixtureSequence();
	Sequence->GetPackage()->SetDirtyFlag(false);

	const FSequenceTrackResult Removed = USequenceTrackService::RemoveMaterialParameter(SequenceTestPath, TEXT("Cloud_GlobalCoverage"), TEXT("Cloud"));
	TestTrue(FString::Printf(TEXT("removal on Cloud succeeds (%s)"), *Removed.Message), Removed.bSuccess);
	TestEqual(TEXT("one entry removed"), Removed.RemovedCount, 1);
	TestTrue(TEXT("the package is dirty"), Sequence->GetPackage()->IsDirty());

	TArray<FSequenceMaterialParameterInfo> Infos = USequenceTrackService::ListMaterialParameters(SequenceTestPath);
	TestNull(TEXT("Cloud no longer animates Cloud_GlobalCoverage"), FindParameter(Infos, TEXT("Cloud"), TEXT("Cloud_GlobalCoverage")));
	TestNotNull(TEXT("Cloud still animates Cloud_GlobalDensity"), FindParameter(Infos, TEXT("Cloud"), TEXT("Cloud_GlobalDensity")));
	TestNotNull(TEXT("Cloud still animates Tint"), FindParameter(Infos, TEXT("Cloud"), TEXT("Tint")));
	TestNotNull(TEXT("Other still animates Cloud_GlobalCoverage"), FindParameter(Infos, TEXT("Other"), TEXT("Cloud_GlobalCoverage")));

	GEditor->UndoTransaction();
	Infos = USequenceTrackService::ListMaterialParameters(SequenceTestPath);
	const FSequenceMaterialParameterInfo* Restored = FindParameter(Infos, TEXT("Cloud"), TEXT("Cloud_GlobalCoverage"));
	if (TestNotNull(TEXT("undo brings Cloud's Cloud_GlobalCoverage back"), Restored))
	{
		TestEqual(TEXT("with both its keys"), Restored->KeyCount, 2);
	}
	GEditor->RedoTransaction();

	Sequence->GetPackage()->SetDirtyFlag(false);
	const FSequenceTrackResult Missing = USequenceTrackService::RemoveMaterialParameter(SequenceTestPath, TEXT("Cloud_GlobalCoverage"), TEXT("Cloud"));
	TestFalse(TEXT("removing it again fails"), Missing.bSuccess);
	TestEqual(TEXT("and removes nothing"), Missing.RemovedCount, 0);
	TestFalse(TEXT("and leaves the package clean"), Sequence->GetPackage()->IsDirty());

	const FSequenceTrackResult Everywhere = USequenceTrackService::RemoveMaterialParameter(SequenceTestPath, TEXT("Cloud_GlobalCoverage"));
	TestTrue(FString::Printf(TEXT("removal on every binding succeeds (%s)"), *Everywhere.Message), Everywhere.bSuccess);
	TestEqual(TEXT("it removes Other's entry"), Everywhere.RemovedCount, 1);
	TestNull(TEXT("Other no longer animates Cloud_GlobalCoverage"),
		FindParameter(USequenceTrackService::ListMaterialParameters(SequenceTestPath), TEXT("Other"), TEXT("Cloud_GlobalCoverage")));

	TestFalse(TEXT("an empty parameter name is refused"),
		USequenceTrackService::RemoveMaterialParameter(SequenceTestPath, TEXT("")).bSuccess);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
