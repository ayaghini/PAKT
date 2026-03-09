# Execution Playbook

## 1) Work loop (always)
1. Pick next item from `implementation_steps_mvp.md`.
2. Confirm dependencies are done.
3. Implement smallest vertical slice.
4. Add/extend tests in same change.
5. Run relevant checks from `qa_gates.md`.
6. Update status section in the step file.

## 2) Preflight checklist (before coding)
- Confirm target step `Status` is `todo`.
- Confirm no unresolved blocker exists for this step.
- Read only minimum needed source docs.
- State assumptions explicitly in your work notes.
- Define expected evidence to close the step.

## 3) Change size policy
- Prefer 1 backlog item per PR/commit.
- If item is large, split by interface boundary (driver, protocol, UI).

## 4) Escalation policy
Escalate immediately if:
- Hardware behavior contradicts assumptions in interface contracts.
- BLE security requirement cannot be met without breaking compatibility.
- Test gate failures are intermittent and non-reproducible.

## 5) Priority policy
- Always finish P0 before P1.
- For MVP, prioritize: FW-003/004/011/012 + APP-002/003/006 + QA-002/004.

## 6) Output policy
Each completed step must produce:
- Code changes
- Tests
- Short result note: what passed, what remains, risks

## 7) Evidence policy
Minimum evidence per step:
- Build/test output summary.
- Functional check summary mapped to gate IDs (`G0..G4`).
- Residual risk list with owner/next action.

## 8) Failure-handling policy
- If hardware-dependent behavior cannot be validated, mark step `blocked` with reason.
- Never "fake pass" a hardware gate with simulation-only evidence.
- Keep `PTT=off` as default and fallback state on all error paths.
