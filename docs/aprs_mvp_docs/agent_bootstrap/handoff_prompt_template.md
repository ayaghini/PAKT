# Handoff Prompt Template (Low-Token)

Use this when passing work to another AI agent.

```
You are continuing PAKT implementation.
Load only these files in order:
1) docs/aprs_mvp_docs/agent_bootstrap/agent_context.yaml
2) docs/aprs_mvp_docs/agent_bootstrap/execution_playbook.md
3) docs/aprs_mvp_docs/agent_bootstrap/architecture_contracts.md
4) docs/aprs_mvp_docs/agent_bootstrap/implementation_steps_mvp.md
5) docs/aprs_mvp_docs/agent_bootstrap/qa_gates.md
6) docs/aprs_mvp_docs/agent_bootstrap/device_loop.md (only if hardware is connected)

Task:
- Execute the next `todo` step only.
- Implement code + tests.
- Run applicable QA gates.
- Update step status and provide a short completion report with residual risks.

Constraints:
- Keep protocol backward compatible unless explicitly instructed.
- Fail safe on TX/PTT behavior.
- Do not skip security requirements for BLE writes.

Required handoff report format:
- Step executed:
- Status set in `implementation_steps_mvp.md`:
- Files changed:
- Tests run and result:
- QA gates checked (`G0..G4`) and result:
- Residual risks:
- Blockers (if any):
- Recommended next step:
```
