# Native farm overlay state

ADR-073 restores the farm overlay on a second activation, including returning
to the title and loading again. Normal native builds need no Python or AST pass.
CMake compiles native-only wrappers that include the unchanged original TU and
its object inventory. Existing field/status SDK call-site definitions are copied
to their wrappers. Source order is preserved.

The checked inventory covers all 51 farm TUs and 705 file-scope objects, including
the TU-local section table defined by a shared header. C type classification
excludes 26 top-level const objects; the 679 mutable objects total 49,209 bytes in
the validated Linux x86-64 build. Copy individual C objects, never linker ranges
or sanitizer redzones. Resident game_state, farm_area, settings and save files
are outside this inventory.

The 56 function-local static tables are all FarmCueStep arrays. Their addresses
escape only to farm_cue_step_run, which reads the table and changes actor/step/
timer state. They never change, so reloading them has no effect. local_tables.json
records this exception. The generator rejects other local statics or consumers;
the reentry fixture calls both consumer location branches using read-only storage.
Changes to that consumer or the cue macros must be reviewed when re-auditing.

Source and header fingerprints fail configuration if this audited input changes
or a farm TU is added/removed. Re-audit with a Clang native compilation database:

```powershell
python port/analysis/inventory_farm_state.py build/port/windows-release/compile_commands.json
python port/analysis/inventory_farm_state.py build/port/windows-release/compile_commands.json --check
```

Generate the database using CMAKE_EXPORT_COMPILE_COMMANDS=ON on an existing port
build. Python 3.10+ and the configured Clang are only required for this maintenance
tool. Review regenerated inventories and the read-only local-table exception;
updating a fingerprint alone does not establish correctness. No disc or SDK is
needed. Linux/macOS/Android builds consume the same inventories but were not
validated in this Windows session.
