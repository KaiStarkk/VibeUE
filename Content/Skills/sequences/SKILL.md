---
name: sequences
display_name: Sequence Tracks
description: Inspect and edit the material tracks of Level Sequences, Day Sequences and other movie scene sequences — list the material parameters a sequence animates and remove one, with its curve (SequenceTrackService). Use when a sequence keys a material parameter that something else should own, or to audit what a sequence animates.
vibeue_classes:
  - SequenceTrackService
unreal_classes:
  - MovieSceneSequence
  - LevelSequence
  - DaySequence
  - MovieSceneComponentMaterialTrack
  - MovieSceneComponentMaterialParameterSection
  - MovieSceneParameterSection
keywords:
  - sequencer
  - level sequence
  - day sequence
  - material track
  - material parameter
  - keys
  - curve
related_skills:
  - asset-management
  - materials
---

# Sequence Tracks Skill

## Service Access

```python
import unreal
svc = unreal.SequenceTrackService
```

All methods are static and take the sequence's asset path, e.g. `/Game/Environment/Soharat/Sky/DS_SoharatSky`.

## Listing what a sequence animates

```python
for p in unreal.SequenceTrackService.list_material_parameters(path):
    print(p.binding_name, "|", p.track_name, "|", p.section_index, p.parameter_name, p.parameter_type, p.key_count)
```

`binding_name` is the binding's display name as Sequencer shows it ("Volumetric Cloud"); it is empty for a root track.

## Removing a parameter

```python
r = unreal.SequenceTrackService.remove_material_parameter(path, "Cloud_GlobalCoverage", "Volumetric Cloud")
print(r.success, r.removed_count, r.message)
if r.success:
    unreal.SequenceTrackService.save_sequence(path)
```

- The parameter's entry is removed with its curve from every section of every material track on that binding
  (every binding and the root when `binding_name` is empty), as one undoable transaction.
- Removing the entry, not just its keys, is what stops the sequence writing the parameter: a channel with no keys can
  still apply its default value every evaluation.
- `success` is false, and nothing is changed, when no matching parameter exists.
- Call `save_sequence()` to persist the change.
