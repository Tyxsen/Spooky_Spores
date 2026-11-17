"""Tests for guard_unreal.py. Run: python -I .claude/hooks/test_guard_unreal.py [path/to/guard_unreal.py]"""
import json, os, subprocess, sys
script = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "guard_unreal.py")
T = "mcp__unreal-engine__unreal"
def ex(cap, tool, action, params=None, with_cap=True):
    ti = {"operation": "execute", "tool": tool, "action": action, "params": params or {}}
    if with_cap: ti["capability"] = cap
    return {"tool_name": T, "tool_input": ti}
CASES = [
 # (profile, event, expected exit code, label)
 ("read", {"tool_name": T, "tool_input": {"operation": "search", "query": "x"}}, 0, "search ok"),
 ("read", {"tool_name": T, "tool_input": {"operation": "describe", "tool": "manage_blueprint", "action": "create"}}, 0, "describe ok"),
 ("read", {"tool_name": T, "tool_input": {"operation": "configure", "action": "x"}}, 2, "configure denied"),
 ("read", ex("asset.query_asset", "manage_asset", "query_asset"), 0, "read cap ok"),
 ("read", ex("system_control.execute_python", "system_control", "execute_python"), 2, "python denied"),
 ("read", ex("control_editor.set_camera", "control_editor", "set_camera"), 2, "set_camera (mislabelled read) denied"),
 ("read", ex("blueprint.create", "manage_blueprint", "create"), 2, "write cap denied under read"),
 ("read", ex(None, "manage_blueprint", "create", with_cap=False), 2, "write via tool/action only denied under read"),
 ("read", ex("asset.query_asset", "manage_blueprint", "create"), 2, "capability/tool mismatch denied"),
 ("read", ex("inspect.query_object", "inspect", "query_object", {"outputPath": "x.t3d"}), 2, "query_object outputPath denied"),
 ("read", ex("inspect.query_object", "inspect", "query_object", {"lookup": "list"}), 0, "query_object list ok"),
 ("write", ex("blueprint.create", "manage_blueprint", "create"), 0, "write cap ok"),
 ("write", ex(None, "manage_blueprint", "create", with_cap=False), 0, "write via tool/action ok"),
 ("write", ex("blueprint.edit_variable", "manage_blueprint", "edit_variable", {"edit": "add_variable"}), 2, "edit_variable add denied"),
 ("write", ex("blueprint.edit_variable", "manage_blueprint", "edit_variable", {"edit": "set_default"}), 0, "edit_variable set_default ok"),
 ("write", ex("blueprint.edit_variable", "manage_blueprint", "edit_variable"), 2, "edit_variable without edit denied"),
 ("write", ex("control_actor.edit_component", "control_actor", "edit_component", {"edit": "remove"}), 2, "edit_component remove denied"),
 ("write", ex("control_actor.edit_component", "control_actor", "edit_component", {"edit": "add"}), 0, "edit_component add ok"),
 ("write", ex("control_editor.save_all", "control_editor", "save_all"), 2, "save_all untargeted denied"),
 ("write", ex("control_editor.save_all", "control_editor", "save_all", {"assetPaths": []}), 2, "save_all empty denied"),
 ("write", ex("control_editor.save_all", "control_editor", "save_all", {"assetPaths": ["/Game/X"]}), 0, "save_all targeted ok"),
 ("write", ex("blueprint.edit_widget_blueprint", "manage_blueprint", "edit_widget_blueprint", {"edit": "create"}), 0, "widget structure ok"),
] + [("write", ex(c, t, a), 2, c + " denied") for c, t, a in [
 ("system_control.execute_python", "system_control", "execute_python"),
 ("control_editor.console_command", "control_editor", "console_command"),
 ("system_control.console_command", "system_control", "console_command"),
 ("control_editor.call_editor_tool", "control_editor", "call_editor_tool"),
 ("control_editor.call_editor_tool_destructive", "control_editor", "call_editor_tool_destructive"),
 ("control_editor.play", "control_editor", "play"),
 ("system_control.run_build", "system_control", "run_build"),
 ("system_control.set_project_setting", "system_control", "set_project_setting"),
 ("asset.delete", "manage_asset", "delete"),
 ("control_actor.delete", "control_actor", "delete"),
 ("manage_effect.cleanup", "manage_effect", "cleanup"),
 ("blueprint.edit_graph", "manage_blueprint", "edit_graph"),
 ("blueprint.add_behaviour", "manage_blueprint", "add_behaviour"),
 ("blueprint.bind_widget", "manage_blueprint", "bind_widget"),
 ("blueprint.add_game_widget", "manage_blueprint", "add_game_widget"),
 ("manage_level_structure.edit_level_blueprint", "manage_level_structure", "edit_level_blueprint"),
 ("manage_level.create_level", "manage_level", "create_level"),
]]
def run(profile, stdin):
    return subprocess.run([sys.executable, "-I", script, profile], input=stdin, capture_output=True, text=True).returncode
bad = 0
for profile, ev, want, label in CASES:
    got = run(profile, json.dumps(ev))
    if got != want: bad += 1; print("FAIL", label, "want", want, "got", got)
for profile, stdin, want, label in [("read", "not json", 2, "garbage stdin denied"), ("bogus", "{}", 2, "unknown profile denied"),
                                    ("read", json.dumps({"tool_name": "Bash", "tool_input": {}}), 0, "other tool ignored"),
 ("read", json.dumps({"tool_name": T, "tool_input": {"operation": "execute", "capability": ["x"]}}), 2, "crash-inducing input denied (not exit 1)"),
 ("read", json.dumps({"tool_name": T, "tool_input": {"operation": ["x"]}}), 2, "unhashable operation denied (not exit 1)")]:
    got = run(profile, stdin)
    if got != want: bad += 1; print("FAIL", label, "want", want, "got", got)
n = len(CASES) + 5
print("%d/%d passed" % (n - bad, n)); sys.exit(1 if bad else 0)
