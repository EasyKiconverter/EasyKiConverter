# Agent JSON Tools

[中文版](AGENT_TOOLS.md)

This tool layer gives Agents structured, reviewable, and permission-limited project interfaces. The implementation is [`tools/python/agent_tools.py`](../../../tools/python/agent_tools.py). It reuses the existing change classifier, verification policy, AI documentation validator, EDA capability ledger, and fixture provenance instead of maintaining a second rule set.

## Invocation

```bash
python3 tools/python/agent_tools.py <tool> [arguments]
```

Every result is JSON and uses the exit code to report `ok`. The tools do not access the network, commit, push, or rewrite Git history.

| Tool | Purpose | Default side effect |
| --- | --- | --- |
| `inspect_changes` | Inspect baseline, HEAD, worktree, and complete change classification | Read-only |
| `plan_verification` | Generate steps, reasons, and risks from the classification and policy | Planning only |
| `validate_project_docs` | Validate AI docs, Skills, workflows, capability and fixture metadata | Read-only |
| `query_capability` | Query code, tests, structural validation, and commercial-tool evidence for an artifact | Read-only |
| `query_fixture` | Query fixture provenance, hash, test use, and invariants | Read-only |
| `run_check` | Run fixed allowlisted formatting, docs, Python, build, or CTest checks | Allowlisted checks only |
| `generate_evidence_report` | Generate a structured Evidence Report from actual supplied results | stdout by default; `--output` writes inside the repository, and overwriting requires explicit `--force` |

## Security boundaries

- `run_check` accepts only fixed check names; it does not accept arbitrary shell commands, scripts, or test paths.
- The tools do not access the network or commercial EDA software, and never convert automated tests into commercial compatibility claims.
- Inspection, query, and validation tools are read-only; `run_check` may create build artifacts and logs.
- `generate_evidence_report` does not invent missing facts; absent commercial EDA evidence remains `unknown`.
- Evidence Report output must remain inside the repository; existing files are protected unless `--force` is explicitly provided.
- The Python CLI is the current stable interface. An MCP exposure layer is a later phase and cross-Agent automatic discovery is not claimed.

## Examples

```bash
python3 tools/python/agent_tools.py inspect_changes \
  --base origin/v3.1.13 --head HEAD
python3 tools/python/agent_tools.py plan_verification --classification full
python3 tools/python/agent_tools.py query_capability --format kicad --artifact symbol
python3 tools/python/agent_tools.py run_check --name ai_consistency
```

The `plan_verification` classification selects the minimum validation scope only. Importer, Exporter, IR, and EDA-specific work still requires module-level validation from the testing guide and capability ledger.
