# Agent Bootstrap Pack (Low-Token)

Purpose: get a new AI coding agent productive fast with minimal context.

Load order (strict):
1. `agent_context.yaml`
2. `execution_playbook.md`
3. `architecture_contracts.md`
4. `implementation_steps_mvp.md`
5. `qa_gates.md`
6. `handoff_prompt_template.md` (only when handing to another agent)

Primary source docs (only open if blocked):
- `../docs/02_mvp_scope.md`
- `../docs/05_ble_gatt_spec.md`
- `../docs/06_firmware_architecture.md`
- `../docs/08_test_plan.md`
- `../docs/12_implementation_backlog.md`
- `step_source_map.md` (which docs to open per step)

Rules:
- Prefer implementing one step at a time with tests in the same change.
- Never skip `qa_gates.md` before closing a step.
- Keep protocol backward compatible unless explicitly approved.

## What this pack must provide
- Clear defaults so agents do not invent radio/BLE behavior.
- Explicit stop conditions when hardware assumptions are violated.
- A repeatable loop for "implement -> test -> evidence -> handoff".

## Agent operating model
1. Load the six files above in order.
2. Execute exactly one `todo` step from `implementation_steps_mvp.md`.
3. Keep changes scoped to that step and its direct dependencies.
4. Run applicable QA gates and collect evidence.
5. Update step status and produce handoff report.

## Required outputs per step
- Code/config/docs change set.
- Tests for touched behavior.
- Short completion report:
  - what changed
  - what passed
  - what is still risky
  - what next agent should do

## Blockers and escalation
Escalate (do not guess) when:
- SGTL5000 clocking constraints cannot be satisfied as documented.
- SA818 electrical behavior differs from assumptions.
- BLE security requirements conflict with compatibility.
- Gate failures are intermittent and not reproducible.
