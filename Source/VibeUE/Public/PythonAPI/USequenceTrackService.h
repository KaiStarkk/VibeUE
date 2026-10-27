// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "USequenceTrackService.generated.h"

// ============================================================================
// RESULT / INFO STRUCTS
// ============================================================================

/** Result returned by mutating sequence-track operations */
USTRUCT(BlueprintType)
struct FSequenceTrackResult
{
	GENERATED_BODY()

	/** Whether the operation succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	bool bSuccess = false;

	/** Human-readable result message */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	FString Message;

	/** How many parameter entries were removed, across every matching track and section */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	int32 RemovedCount = 0;
};

/** One animated material parameter in one section of a material track */
USTRUCT(BlueprintType)
struct FSequenceMaterialParameterInfo
{
	GENERATED_BODY()

	/** Display name of the binding that owns the track, e.g. "Volumetric Cloud"; empty for a root track */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	FString BindingName;

	/** Display name of the material track, e.g. "Volumetric Cloud Material m_SimpleVolumetricCloud_Inst" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	FString TrackName;

	/** UClass name of the track, e.g. "MovieSceneComponentMaterialTrack" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	FString TrackClass;

	/** 0-based index of the section within its track */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	int32 SectionIndex = -1;

	/** Material parameter name, e.g. "Cloud_GlobalCoverage" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	FString ParameterName;

	/** "Scalar" or "Color" (component material sections); legacy parameter sections also report "Bool", "Vector2D", "Vector" and "Transform" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	FString ParameterType;

	/** Keys on the parameter's channel (the most on any of a color's channels) */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Sequencer")
	int32 KeyCount = 0;
};

/**
 * Sequence track service: inspect and edit the material tracks of Level Sequences, Day Sequences and any other
 * UMovieSceneSequence.
 *
 * A material track animates parameters of a material on its bound object (a component's material slot, a decal, an
 * overlay). Each parameter is an entry in one of its sections, with its own curve. Removing a parameter removes the
 * entry and its curve, so the sequence stops writing that parameter at all; clearing its keys alone would leave the
 * channel's default value still applied every evaluation.
 *
 * **C++ Source:**
 *
 * - **Plugin**: VibeUE
 * - **Module**: VibeUE
 * - **File**: USequenceTrackService.h
 */
UCLASS(BlueprintType)
class VIBEUE_API USequenceTrackService : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	/**
	 * List every animated material parameter in a sequence's material tracks, on bindings and at the root.
	 *
	 * @param SequencePath - Full path to the sequence asset, e.g. "/Game/Environment/Soharat/Sky/DS_SoharatSky"
	 * @return One entry per parameter per section; empty if the asset is missing or has no material tracks
	 *
	 * Example:
	 *   for p in unreal.SequenceTrackService.list_material_parameters("/Game/Environment/Soharat/Sky/DS_SoharatSky"):
	 *       print(p.binding_name, p.parameter_name, p.parameter_type, p.key_count)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Sequencer|Inspection")
	static TArray<FSequenceMaterialParameterInfo> ListMaterialParameters(const FString& SequencePath);

	/**
	 * Remove an animated parameter, with its curve, from every section of a sequence's material tracks.
	 * The edit is one undoable transaction; call save_sequence() afterwards to persist it.
	 *
	 * @param SequencePath  - Full path to the sequence asset
	 * @param ParameterName - Material parameter to remove, e.g. "Cloud_GlobalCoverage"
	 * @param BindingName   - Only tracks on the binding with this display name; empty for every binding and the root
	 * @return FSequenceTrackResult; bSuccess is false when no matching parameter was found
	 *
	 * Example:
	 *   r = unreal.SequenceTrackService.remove_material_parameter(
	 *       "/Game/Environment/Soharat/Sky/DS_SoharatSky", "Cloud_GlobalCoverage", "Volumetric Cloud")
	 *   print(r.success, r.removed_count, r.message)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Sequencer|Tracks")
	static FSequenceTrackResult RemoveMaterialParameter(
		const FString& SequencePath,
		const FString& ParameterName,
		const FString& BindingName = TEXT(""));

	/**
	 * Save a sequence asset to disk.
	 *
	 * @param SequencePath - Full path to the sequence asset
	 * @return True if saved successfully
	 *
	 * Example:
	 *   unreal.SequenceTrackService.save_sequence("/Game/Environment/Soharat/Sky/DS_SoharatSky")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Sequencer|Lifecycle")
	static bool SaveSequence(const FString& SequencePath);
};
