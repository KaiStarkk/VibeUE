// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/USequenceTrackService.h"

#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Sections/MovieSceneParameterSection.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SequenceTrackService"

namespace
{
	UMovieSceneSequence* LoadSequence(const FString& SequencePath)
	{
		if (SequencePath.IsEmpty() || !UEditorAssetLibrary::DoesAssetExist(SequencePath))
		{
			return nullptr;
		}
		return Cast<UMovieSceneSequence>(UEditorAssetLibrary::LoadAsset(SequencePath));
	}

	/** A material track with the display name of the binding it sits on (empty for a root track). */
	struct FMaterialTrackRef
	{
		UMovieSceneMaterialTrack* Track;
		FString BindingName;
	};

	TArray<FMaterialTrackRef> GatherMaterialTracks(UMovieScene* MovieScene)
	{
		TArray<FMaterialTrackRef> Out;
		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			if (UMovieSceneMaterialTrack* MaterialTrack = Cast<UMovieSceneMaterialTrack>(Track))
			{
				Out.Add({ MaterialTrack, FString() });
			}
		}
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			const FString BindingName = MovieScene->GetObjectDisplayName(Binding.GetObjectGuid()).ToString();
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				if (UMovieSceneMaterialTrack* MaterialTrack = Cast<UMovieSceneMaterialTrack>(Track))
				{
					Out.Add({ MaterialTrack, BindingName });
				}
			}
		}
		return Out;
	}

	int32 MaxKeys(std::initializer_list<const FMovieSceneFloatChannel*> Channels)
	{
		int32 Keys = 0;
		for (const FMovieSceneFloatChannel* Channel : Channels)
		{
			Keys = FMath::Max(Keys, Channel->GetNumKeys());
		}
		return Keys;
	}
}

TArray<FSequenceMaterialParameterInfo> USequenceTrackService::ListMaterialParameters(const FString& SequencePath)
{
	TArray<FSequenceMaterialParameterInfo> Out;
	UMovieSceneSequence* Sequence = LoadSequence(SequencePath);
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return Out;
	}

	for (const FMaterialTrackRef& Ref : GatherMaterialTracks(MovieScene))
	{
		const TArray<UMovieSceneSection*>& Sections = Ref.Track->GetAllSections();
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			auto Add = [&](FName Name, const TCHAR* Type, int32 Keys)
			{
				FSequenceMaterialParameterInfo& Info = Out.AddDefaulted_GetRef();
				Info.BindingName = Ref.BindingName;
				Info.TrackName = Ref.Track->GetDisplayName().ToString();
				Info.TrackClass = Ref.Track->GetClass()->GetName();
				Info.SectionIndex = SectionIndex;
				Info.ParameterName = Name.ToString();
				Info.ParameterType = Type;
				Info.KeyCount = Keys;
			};

			if (const UMovieSceneComponentMaterialParameterSection* Section = Cast<UMovieSceneComponentMaterialParameterSection>(Sections[SectionIndex]))
			{
				for (const FScalarMaterialParameterInfoAndCurve& P : Section->ScalarParameterInfosAndCurves)
				{
					Add(P.ParameterInfo.Name, TEXT("Scalar"), P.ParameterCurve.GetNumKeys());
				}
				for (const FColorMaterialParameterInfoAndCurves& P : Section->ColorParameterInfosAndCurves)
				{
					Add(P.ParameterInfo.Name, TEXT("Color"), MaxKeys({ &P.RedCurve, &P.GreenCurve, &P.BlueCurve, &P.AlphaCurve }));
				}
			}
			else if (UMovieSceneParameterSection* Legacy = Cast<UMovieSceneParameterSection>(Sections[SectionIndex]))
			{
				for (const FScalarParameterNameAndCurve& P : Legacy->GetScalarParameterNamesAndCurves())
				{
					Add(P.ParameterName, TEXT("Scalar"), P.ParameterCurve.GetNumKeys());
				}
				for (const FBoolParameterNameAndCurve& P : Legacy->GetBoolParameterNamesAndCurves())
				{
					Add(P.ParameterName, TEXT("Bool"), P.ParameterCurve.GetNumKeys());
				}
				for (const FVector2DParameterNameAndCurves& P : Legacy->GetVector2DParameterNamesAndCurves())
				{
					Add(P.ParameterName, TEXT("Vector2D"), MaxKeys({ &P.XCurve, &P.YCurve }));
				}
				for (const FVectorParameterNameAndCurves& P : Legacy->GetVectorParameterNamesAndCurves())
				{
					Add(P.ParameterName, TEXT("Vector"), MaxKeys({ &P.XCurve, &P.YCurve, &P.ZCurve }));
				}
				for (const FColorParameterNameAndCurves& P : Legacy->GetColorParameterNamesAndCurves())
				{
					Add(P.ParameterName, TEXT("Color"), MaxKeys({ &P.RedCurve, &P.GreenCurve, &P.BlueCurve, &P.AlphaCurve }));
				}
				for (const FTransformParameterNameAndCurves& P : Legacy->GetTransformParameterNamesAndCurves())
				{
					Add(P.ParameterName, TEXT("Transform"), MaxKeys({ &P.Translation[0], &P.Translation[1], &P.Translation[2],
						&P.Rotation[0], &P.Rotation[1], &P.Rotation[2], &P.Scale[0], &P.Scale[1], &P.Scale[2] }));
				}
			}
		}
	}
	return Out;
}

FSequenceTrackResult USequenceTrackService::RemoveMaterialParameter(const FString& SequencePath, const FString& ParameterName, const FString& BindingName)
{
	FSequenceTrackResult Result;
	UMovieSceneSequence* Sequence = LoadSequence(SequencePath);
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		Result.Message = FString::Printf(TEXT("Not a sequence asset: '%s'"), *SequencePath);
		return Result;
	}
	if (ParameterName.IsEmpty())
	{
		Result.Message = TEXT("ParameterName must not be empty");
		return Result;
	}
	if (MovieScene->IsReadOnly())
	{
		Result.Message = FString::Printf(TEXT("Sequence is read-only: '%s'"), *SequencePath);
		return Result;
	}

	const FName Name(*ParameterName);
	FScopedTransaction Transaction(FText::Format(LOCTEXT("RemoveMaterialParameter", "Remove Material Parameter {0}"), FText::FromString(ParameterName)));

	for (const FMaterialTrackRef& Ref : GatherMaterialTracks(MovieScene))
	{
		if (!BindingName.IsEmpty() && Ref.BindingName != BindingName)
		{
			continue;
		}
		for (UMovieSceneSection* Section : Ref.Track->GetAllSections())
		{
			if (UMovieSceneComponentMaterialParameterSection* ComponentSection = Cast<UMovieSceneComponentMaterialParameterSection>(Section))
			{
				// Each entry's own parameter info, so a layered parameter of the same name is matched exactly.
				TArray<FMaterialParameterInfo> Scalars, Colors;
				for (const FScalarMaterialParameterInfoAndCurve& P : ComponentSection->ScalarParameterInfosAndCurves)
				{
					if (P.ParameterInfo.Name == Name) { Scalars.Add(P.ParameterInfo); }
				}
				for (const FColorMaterialParameterInfoAndCurves& P : ComponentSection->ColorParameterInfosAndCurves)
				{
					if (P.ParameterInfo.Name == Name) { Colors.Add(P.ParameterInfo); }
				}
				if (Scalars.Num() + Colors.Num() > 0)
				{
					ComponentSection->Modify();
				}
				for (const FMaterialParameterInfo& Info : Scalars)
				{
					Result.RemovedCount += ComponentSection->RemoveScalarParameter(Info) ? 1 : 0;
				}
				for (const FMaterialParameterInfo& Info : Colors)
				{
					Result.RemovedCount += ComponentSection->RemoveColorParameter(Info) ? 1 : 0;
				}
			}
			else if (UMovieSceneParameterSection* Legacy = Cast<UMovieSceneParameterSection>(Section))
			{
				TSet<FName> Names;
				Legacy->GetParameterNames(Names);
				if (!Names.Contains(Name))
				{
					continue;
				}
				Legacy->Modify();
				Result.RemovedCount += Legacy->RemoveScalarParameter(Name) ? 1 : 0;
				Result.RemovedCount += Legacy->RemoveBoolParameter(Name) ? 1 : 0;
				Result.RemovedCount += Legacy->RemoveVector2DParameter(Name) ? 1 : 0;
				Result.RemovedCount += Legacy->RemoveVectorParameter(Name) ? 1 : 0;
				Result.RemovedCount += Legacy->RemoveColorParameter(Name) ? 1 : 0;
				Result.RemovedCount += Legacy->RemoveTransformParameter(Name) ? 1 : 0;
			}
		}
	}

	if (Result.RemovedCount == 0)
	{
		Transaction.Cancel();
		Result.Message = BindingName.IsEmpty()
			? FString::Printf(TEXT("No material track in '%s' animates '%s'"), *SequencePath, *ParameterName)
			: FString::Printf(TEXT("No material track on binding '%s' in '%s' animates '%s'"), *BindingName, *SequencePath, *ParameterName);
		return Result;
	}

	MovieScene->MarkPackageDirty();
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Removed '%s' from %d section entr%s"), *ParameterName, Result.RemovedCount, Result.RemovedCount == 1 ? TEXT("y") : TEXT("ies"));
	return Result;
}

bool USequenceTrackService::SaveSequence(const FString& SequencePath)
{
	if (!LoadSequence(SequencePath))
	{
		return false;
	}
	return UEditorAssetLibrary::SaveAsset(SequencePath, false);
}

#undef LOCTEXT_NAMESPACE
