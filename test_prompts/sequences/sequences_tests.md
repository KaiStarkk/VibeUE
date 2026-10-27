# Sequence Track Tests — Material Parameters

List and remove the material parameters a sequence animates. Run sequentially, on a copy of a sequence whose
bindings have a material track (a Day Sequence's Volumetric Cloud binding, or any Level Sequence animating a
material parameter).

---

## Setup

Duplicate the sequence to /Game/SequenceTests/DS_Test_Copy.

---

## List

Which material parameters does DS_Test_Copy animate? For each, tell me its binding, its type and how many keys it has.

---

## Remove

Remove one of the scalar parameters from its binding in DS_Test_Copy, then list the material parameters again.
The removed parameter should be gone from that binding only; every other parameter stays, with the same keys.

---

Remove the same parameter from the same binding again. It should report that nothing animates it there and change nothing.

---

## Undo

Undo the removal and list the parameters again: the parameter is back, with all its keys. Redo it.

---

## Save

Save DS_Test_Copy, then delete it.
