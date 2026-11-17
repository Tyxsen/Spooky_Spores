#!/usr/bin/env python3
"""PreToolUse guard for the Unreal MCP server (single tool `unreal`).

Usage (from a sub-agent frontmatter hook): guard_unreal.py <profile>
  read  : read-only capabilities (bp-integration-planner, editor-verifier)
  write : read + the editor writes a checklist may need (bp-integrator)

Why this exists: the server exposes ONE tool, so the `tools:` allowlist can no
longer separate reads from writes. The server's own `effect` label is not
reliable either (set_camera and set_viewport_resolution are labelled "read"),
so we allowlist capability ids explicitly.

Fail closed: anything unparsable or unknown is denied (exit code 2, reason on
stderr). This is a fine filter on top of the agent's `tools:` list, not a
security boundary.
"""
import json
import sys

TOOL_NAME = "mcp__unreal-engine__unreal"

# capability id -> (parent tool, action), as returned by the server's catalog.
READ = {
    "system_control.get_project_settings": ("system_control", "get_project_settings"),
    "system_control.screenshot": ("system_control", "screenshot"),
    "inspect.get_editor_state": ("inspect", "get_editor_state"),
    "inspect.query_object": ("inspect", "query_object"),
    "inspect.get_components": ("inspect", "get_components"),
    "inspect.find_by_class": ("inspect", "find_by_class"),
    "asset.query_asset": ("manage_asset", "query_asset"),
    "asset.inspect_asset": ("manage_asset", "inspect_asset"),
    "material.get_material_info": ("manage_asset", "get_material_info"),
    "control_actor.list": ("control_actor", "list"),
    "control_actor.find": ("control_actor", "find"),
    "control_actor.find_by_tag": ("control_actor", "find_by_tag"),
    "control_actor.get_components": ("control_actor", "get_components"),
    "control_actor.get_transform": ("control_actor", "get_transform"),
    "blueprint.get_blueprint": ("manage_blueprint", "get_blueprint"),
    "blueprint.get_widget_info": ("manage_blueprint", "get_widget_info"),
}

WRITE = {
    "asset.create_folder": ("manage_asset", "create_folder"),
    "asset.duplicate": ("manage_asset", "duplicate"),
    "asset.import": ("manage_asset", "import"),
    "blueprint.create": ("manage_blueprint", "create"),
    "blueprint.edit_scs": ("manage_blueprint", "edit_scs"),
    "blueprint.edit_variable": ("manage_blueprint", "edit_variable"),
    # Widget STRUCTURE only. bind_widget (creates functions) and add_game_widget
    # (prebuilt HUD pieces, content unknown) are deliberately absent.
    "blueprint.edit_widget_blueprint": ("manage_blueprint", "edit_widget_blueprint"),
    "blueprint.add_panel_widget": ("manage_blueprint", "add_panel_widget"),
    "blueprint.add_content_widget": ("manage_blueprint", "add_content_widget"),
    "blueprint.set_widget_layout": ("manage_blueprint", "set_widget_layout"),
    "blueprint.duplicate_widget": ("manage_blueprint", "duplicate_widget"),
    "inspect.set_property": ("inspect", "set_property"),
    "inspect.set_component_property": ("inspect", "set_component_property"),
    "control_actor.edit_component": ("control_actor", "edit_component"),
    "control_actor.set_blueprint_variables": ("control_actor", "set_blueprint_variables"),
    "control_actor.spawn": ("control_actor", "spawn"),
    "control_actor.set_transform": ("control_actor", "set_transform"),
    "control_actor.set_material": ("control_actor", "set_material"),
    "material.create_material": ("manage_asset", "create_material"),
    "material.create_material_instance": ("manage_asset", "create_material_instance"),
    "material.set_material_parameter": ("manage_asset", "set_material_parameter"),
    "control_editor.save_all": ("control_editor", "save_all"),
}

# capability id -> (param, allowed values): narrows a wide capability.
ENUM_CONSTRAINTS = {
    "blueprint.edit_variable": ("edit", {"set_default"}),
    "control_actor.edit_component": ("edit", {"add", "set_property", "set_properties"}),
}

# capability id -> params that must be present and non-empty.
REQUIRED_PARAMS = {
    "control_editor.save_all": ["assetPaths"],  # targeted save only
}

# capability id -> params that must be absent (they make a read write a file).
FORBIDDEN_PARAMS = {
    "inspect.query_object": ["outputPath"],
}


def deny(reason):
    sys.stderr.write("guard_unreal: " + reason + "\n")
    sys.exit(2)


def main():
    profile = sys.argv[1] if len(sys.argv) > 1 else ""
    if profile == "read":
        allowed = dict(READ)
    elif profile == "write":
        allowed = {**READ, **WRITE}
    else:
        deny("unknown profile %r (expected 'read' or 'write')" % profile)

    try:
        event = json.load(sys.stdin)
    except Exception as exc:
        deny("cannot parse hook input: %s" % exc)

    if event.get("tool_name") != TOOL_NAME:
        return  # not our tool, nothing to guard

    tool_input = event.get("tool_input")
    if not isinstance(tool_input, dict):
        deny("tool_input missing or not an object")

    operation = tool_input.get("operation")
    if operation in ("search", "describe"):
        return  # discovery has no effect on the editor
    if operation != "execute":
        deny("operation %r is not allowed (configure would change which tool "
             "groups the server exposes)" % operation)

    capability = tool_input.get("capability")
    tool = tool_input.get("tool")
    action = tool_input.get("action")

    if capability:
        entry = allowed.get(capability)
        if entry is None:
            deny("capability %r is not in the %r allowlist" % (capability, profile))
        if (tool and tool != entry[0]) or (action and action != entry[1]):
            deny("tool/action %s/%s does not match capability %r" % (tool, action, capability))
    else:
        capability = next((c for c, pair in allowed.items() if pair == (tool, action)), None)
        if capability is None:
            deny("%s/%s is not in the %r allowlist" % (tool, action, profile))

    params = tool_input.get("params")
    if params is None:
        params = {}
    if not isinstance(params, dict):
        deny("params is not an object")

    if capability in ENUM_CONSTRAINTS:
        name, values = ENUM_CONSTRAINTS[capability]
        if params.get(name) not in values:
            deny("%s requires %s in %s (got %r)" % (capability, name, sorted(values), params.get(name)))
    for name in REQUIRED_PARAMS.get(capability, []):
        if not params.get(name):
            deny("%s requires a non-empty %r" % (capability, name))
    for name in FORBIDDEN_PARAMS.get(capability, []):
        if name in params:
            deny("%s must not be called with %r" % (capability, name))


if __name__ == "__main__":
    # An uncaught exception exits with code 1, which Claude Code treats as a
    # NON-blocking error (the call would go through). Only exit code 2 blocks,
    # so any crash must be turned into a denial.
    try:
        main()
    except SystemExit:
        raise
    except BaseException as exc:
        deny("unexpected error (%s: %s)" % (type(exc).__name__, exc))
