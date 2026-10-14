#!/usr/bin/env python3
"""Publication boundary regressions, using disposable Git repos and local remotes.

Run with ``python3 -m unittest discover -s tests -p test_publication.py -v``.
No command contacts the network or changes this checkout's Git configuration.
"""

import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[1]
CLI = REPOSITORY / "scripts" / "pub"
MANIFEST = "projects/example/project.yaml"
README = "projects/example/README.md"
EVIDENCE = "projects/example/docs/evidence.md"


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="publication-test-")
        self.addCleanup(self.temp.cleanup)
        self.area = Path(self.temp.name)
        self.root = self.area / "source"
        self.root.mkdir()
        self.environment = os.environ.copy()
        # Disable machine hooks, signing, aliases, and unrelated Git configuration.
        self.environment.update({
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_TERMINAL_PROMPT": "0",
        })
        for key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_CONFIG_COUNT"):
            self.environment.pop(key, None)
        self.git("init", "-q")
        self.git("config", "user.name", "Publication Test")
        self.git("config", "user.email", "publication-test@example.invalid")
        self.manifest = {
            "id": "example",
            "name": "Example",
            "status": "stable",
            "publication": {
                "state": "ready",
                "acceptance": "passed",
                "privacy": "passed",
                "rights": "cleared",
                "targets": ["git"],
                "files": [MANIFEST, README, EVIDENCE],
            },
        }
        self.write(README, "# Example\n\nValidated scope: [evidence](docs/evidence.md).\n")
        self.write(EVIDENCE, "# Evidence\n\nDeclared acceptance criteria passed.\n")
        self.save_manifest()
        self.git("add", "--", MANIFEST, README, EVIDENCE)

    def run_command(self, command, cwd=None, input_text=""):
        return subprocess.run(
            command, cwd=str(cwd or self.root), env=self.environment,
            input=input_text, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=30,
        )

    def git(self, *arguments, expect_success=True, cwd=None):
        result = self.run_command(["git"] + list(arguments), cwd=cwd)
        if expect_success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def pub(self, *arguments, expect_success=True, input_text=""):
        result = self.run_command([str(CLI), "--root", str(self.root), "--json"] + list(arguments),
                                  input_text=input_text)
        if expect_success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def write(self, relative, text):
        target = self.root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
        return target

    def save_manifest(self):
        # JSON is valid YAML and keeps these tests independent of a YAML library.
        self.write(MANIFEST, json.dumps(self.manifest, indent=2) + "\n")

    def review(self, target="git"):
        return json.loads(self.pub("review", MANIFEST, "--target", target).stdout)

    def approve(self, target="git"):
        digest = self.review(target)["sha256"]
        self.manifest["publication"].update({
            "state": "approved",
            "approval": {"sha256": digest, "approved_by": "human-reviewer"},
        })
        self.save_manifest()
        self.pub("--check", MANIFEST, "--target", target)
        return digest

    def commit(self, message="fixture"):
        self.git("add", "--", "projects")
        self.git("commit", "-qm", message)

    def bare_remote(self, name="public"):
        destination = self.area / (name + ".git")
        destination.mkdir()
        self.git("init", "--bare", "-q", cwd=destination)
        self.git("remote", "add", name, str(destination))
        return destination

    def install_hook_fixture(self):
        for directory in ("scripts", ".githooks"):
            source = REPOSITORY / directory
            destination = self.root / directory
            if directory == "scripts":
                destination.mkdir(exist_ok=True)
                for source_file in source.iterdir():
                    if source_file.is_file() and source_file.name != "install-agent-skill.sh":
                        shutil.copy2(str(source_file), str(destination / source_file.name))
            else:
                shutil.copytree(str(source), str(destination))
        self.pub("install-hook")

    def test_missing_publication_and_missing_state_default_to_denied(self):
        for missing in ("publication", "state"):
            with self.subTest(missing=missing):
                original = copy.deepcopy(self.manifest)
                if missing == "publication":
                    del self.manifest["publication"]
                else:
                    del self.manifest["publication"]["state"]
                self.save_manifest()
                self.pub("--check", MANIFEST, expect_success=False)
                self.pub("review", MANIFEST, expect_success=False)
                self.manifest = original
                self.save_manifest()

    def test_ready_can_be_reviewed_but_requires_human_approval(self):
        report = self.review()
        self.assertRegex(report["sha256"], r"^[a-f0-9]{64}$")
        self.assertEqual({entry["path"] for entry in report["files"]}, {MANIFEST, README, EVIDENCE})
        self.pub("--check", MANIFEST, expect_success=False)
        self.manifest["publication"]["state"] = "approved"
        self.save_manifest()
        self.pub("--check", MANIFEST, expect_success=False)

    def test_incomplete_engineering_or_reviews_cannot_be_forged_approved(self):
        self.approve()
        approved = copy.deepcopy(self.manifest)
        for status in ("idea", "prototype", "build-verified", "hardware-verified", "retired"):
            with self.subTest(status=status):
                self.manifest = copy.deepcopy(approved)
                self.manifest["status"] = status
                self.save_manifest()
                self.pub("review", MANIFEST, expect_success=False)
                self.pub("--check", MANIFEST, expect_success=False)
        for field in ("acceptance", "privacy", "rights"):
            with self.subTest(review=field):
                self.manifest = copy.deepcopy(approved)
                self.manifest["publication"][field] = "pending"
                self.save_manifest()
                self.pub("review", MANIFEST, expect_success=False)
                self.pub("--check", MANIFEST, expect_success=False)

    def test_draft_never_passes_and_published_keeps_approved_digest(self):
        digest = self.approve()
        self.manifest["publication"]["state"] = "draft"
        self.save_manifest()
        self.pub("--check", MANIFEST, expect_success=False)
        self.manifest["publication"]["state"] = "published"
        self.save_manifest()
        report = json.loads(self.pub("--check", MANIFEST).stdout)
        self.assertEqual(report["sha256"], digest)

    def test_payload_change_invalidates_approval_and_can_be_reviewed_again(self):
        approved_digest = self.approve()
        self.write(EVIDENCE, "# Evidence\n\nAcceptance scope has changed.\n")
        self.pub("--check", MANIFEST, expect_success=False)
        self.assertNotEqual(self.review()["sha256"], approved_digest)

    def test_scope_metadata_target_and_file_mode_are_bound_to_approval(self):
        self.approve()
        original = copy.deepcopy(self.manifest)
        extra = "projects/example/docs/extra.md"
        self.write(extra, "Additional public evidence.\n")
        self.git("add", "--", extra)
        mutations = {
            "scope": lambda: self.manifest["publication"]["files"].append(extra),
            "metadata": lambda: self.manifest.update({"name": "A changed scope"}),
            "target": lambda: self.manifest["publication"]["targets"].append("platform"),
            "mode": lambda: (self.root / EVIDENCE).chmod(0o755),
        }
        for name, mutate in mutations.items():
            with self.subTest(change=name):
                self.manifest = copy.deepcopy(original)
                (self.root / EVIDENCE).chmod(0o644)
                mutate()
                self.save_manifest()
                self.pub("--check", MANIFEST, expect_success=False)
        self.manifest = copy.deepcopy(original)
        self.save_manifest()
        self.pub("--check", MANIFEST, "--target", "platform", expect_success=False)

    def test_platform_approval_cannot_be_reused_for_git_publication(self):
        self.manifest["publication"]["targets"] = ["git", "platform"]
        self.save_manifest()
        self.approve("platform")
        self.pub("--check", MANIFEST, "--target", "git", expect_success=False)
        destination = self.area / "platform-export"
        self.pub("export", MANIFEST, "--output", str(destination), "--target", "platform")
        self.assertTrue((destination / README).is_file())

    def test_explicit_scope_rejects_directories_patterns_and_path_escapes(self):
        original = copy.deepcopy(self.manifest)
        for unsafe in ("projects/example/docs", "projects/example/docs/*.md", "../outside.md",
                       str(self.area / "outside.md"), "projects/example/../example/README.md"):
            with self.subTest(path=unsafe):
                self.manifest = copy.deepcopy(original)
                self.manifest["publication"]["files"].append(unsafe)
                self.save_manifest()
                self.pub("review", MANIFEST, expect_success=False)

    def test_scope_rejects_symlinks_untracked_ignored_and_private_files(self):
        original = copy.deepcopy(self.manifest)
        symlink = "projects/example/docs/link.md"
        (self.root / symlink).symlink_to("evidence.md")
        self.git("add", "--", symlink)
        untracked = "projects/example/docs/untracked.md"
        self.write(untracked, "Untracked material.\n")
        ignored = "projects/example/docs/ignored.md"
        self.write(".gitignore", ignored + "\n")
        self.write(ignored, "Ignored material.\n")
        self.git("add", "-f", "--", ignored)
        private = "projects/example/.env"
        self.write(private, "LOCAL_SETTING=example\n")
        self.git("add", "--", private)
        for unsafe in (symlink, untracked, ignored, private):
            with self.subTest(path=unsafe):
                self.manifest = copy.deepcopy(original)
                self.manifest["publication"]["files"].append(unsafe)
                self.save_manifest()
                self.pub("review", MANIFEST, expect_success=False)

    def test_credentials_and_local_links_block_review(self):
        original = (self.root / README).read_text(encoding="utf-8")
        safe_extra = "projects/example/docs/unapproved.md"
        self.write(safe_extra, "Safe, but outside this publication's scope.\n")
        self.git("add", "--", safe_extra)
        unsafe_texts = {
            # Build the harmless marker at runtime so this test source can itself
            # pass the publication scan; the generated payload is still denied.
            "private key": "-----BEGIN " + "PRIVATE KEY-----\nMIITestFixtureOnly\n-----END PRIVATE KEY-----\n",
            "missing link": "[missing](docs/missing.md)\n",
            "unapproved link": "[extra](docs/unapproved.md)\n",
            "unapproved image": "![extra](docs/unapproved.md)\n",
            "local file URL": "[private](file:///private/development-note.md)\n",
            "local file URL with host": "[private](file://localhost/private/development-note.md)\n",
        }
        for name, text in unsafe_texts.items():
            with self.subTest(content=name):
                self.write(README, original + "\n" + text)
                self.pub("review", MANIFEST, expect_success=False)

    def test_quoted_json_credentials_are_denied_without_echoing_values(self):
        original = copy.deepcopy(self.manifest)
        value = "SyntheticPrivateValueForRegression"
        credential_field = "wifi_password"
        for location in ("manifest", "payload"):
            with self.subTest(secret=location):
                self.manifest = copy.deepcopy(original)
                if location == "manifest":
                    self.manifest[credential_field] = value
                else:
                    payload = "projects/example/docs/settings.json"
                    self.write(payload, json.dumps({credential_field: value}) + "\n")
                    self.git("add", "--", payload)
                    self.manifest["publication"]["files"].append(payload)
                self.save_manifest()
                result = self.pub("review", MANIFEST, expect_success=False)
                self.assertNotIn(value, result.stdout + result.stderr)

    def write_wrapper(self, scope=None):
        wrapper = "publication/wrapper.yaml"
        data = copy.deepcopy(self.manifest)
        data["id"] = "wrapper"
        data["publication"]["files"] = scope or [wrapper, MANIFEST, README, EVIDENCE]
        self.write(wrapper, json.dumps(data, indent=2) + "\n")
        self.git("add", "--", wrapper)
        return wrapper, data

    def test_wrapper_cannot_override_draft_nonstable_missing_or_omitted_owner(self):
        original = copy.deepcopy(self.manifest)
        wrapper, data = self.write_wrapper()
        self.pub("review", wrapper)
        for owner_problem in ("draft", "prototype", "missing", "omitted"):
            with self.subTest(owner=owner_problem):
                self.manifest = copy.deepcopy(original)
                if owner_problem == "draft":
                    self.manifest["publication"]["state"] = "draft"
                elif owner_problem == "prototype":
                    self.manifest["status"] = "prototype"
                self.save_manifest()
                data["publication"]["files"] = [wrapper, MANIFEST, README, EVIDENCE]
                if owner_problem in {"missing", "omitted"}:
                    data["publication"]["files"].remove(MANIFEST)
                if owner_problem == "missing":
                    (self.root / MANIFEST).unlink()
                self.write(wrapper, json.dumps(data, indent=2) + "\n")
                self.pub("review", wrapper, expect_success=False)

    def test_wrapper_cannot_expand_a_stable_owners_accepted_file_scope(self):
        wrapper, data = self.write_wrapper()
        self.pub("review", wrapper)
        extra = "projects/example/docs/unaccepted-draft.md"
        self.write(extra, "An unfinished addition to the stable project.\n")
        self.git("add", "--", extra)
        data["publication"]["files"].append(extra)
        self.write(wrapper, json.dumps(data, indent=2) + "\n")
        self.pub("review", wrapper, expect_success=False)
        self.manifest["publication"]["files"].append(extra)
        self.save_manifest()
        self.pub("review", wrapper)

    def test_ready_dependency_is_bound_as_exact_payload_in_composite_approval(self):
        wrapper, data = self.write_wrapper()
        digest = json.loads(self.pub("review", wrapper).stdout)["sha256"]
        data["publication"].update({
            "state": "approved",
            "approval": {"sha256": digest, "approved_by": "human-reviewer"},
        })
        self.write(wrapper, json.dumps(data, indent=2) + "\n")
        self.pub("--check", wrapper)
        # Only the selected wrapper's state/approval is excluded from its digest.
        # Foreign manifests must stay literal, including their comments.
        with (self.root / MANIFEST).open("a", encoding="utf-8") as stream:
            stream.write("\n# A dependency edit requires a fresh composite review.\n")
        self.pub("--check", wrapper, expect_success=False)

    def test_export_contains_only_approved_snapshot_without_git_history(self):
        self.write("projects/example/docs/private-note.md", "Unapproved working note.\n")
        self.write("unrelated.txt", "Unrelated material.\n")
        self.approve()
        self.commit()
        destination = self.area / "export"
        self.pub("export", MANIFEST, "--output", str(destination), "--target", "git")
        exported = {path.relative_to(destination).as_posix()
                    for path in destination.rglob("*") if path.is_file()}
        self.assertEqual(exported, {MANIFEST, README, EVIDENCE})
        self.assertFalse((destination / ".git").exists())
        for relative in exported:
            if relative == MANIFEST:
                self.assertEqual(json.loads((destination / relative).read_text()), self.manifest)
            else:
                self.assertEqual((destination / relative).read_bytes(), (self.root / relative).read_bytes())

    def test_export_omits_yaml_comments_added_after_approval(self):
        self.approve()
        with (self.root / MANIFEST).open("a", encoding="utf-8") as stream:
            stream.write("\n# UNREVIEWED-NOTE: this is an unfinished private draft.\n")
        # Formatting/comments are outside the semantic metadata digest and must
        # therefore be absent from the actual exported publication.
        self.pub("--check", MANIFEST)
        destination = self.area / "normalized-export"
        self.pub("export", MANIFEST, "--output", str(destination))
        exported = (destination / MANIFEST).read_text(encoding="utf-8")
        self.assertNotIn("UNREVIEWED-NOTE", exported)
        self.assertEqual(json.loads(exported), self.manifest)

    def test_approval_cannot_carry_unbound_content_or_personal_identity(self):
        self.approve()
        original = copy.deepcopy(self.manifest)
        for change in ("extra content", "personal identity"):
            with self.subTest(approval=change):
                self.manifest = copy.deepcopy(original)
                approval = self.manifest["publication"]["approval"]
                if change == "extra content":
                    approval["unreviewed_note"] = "This unfinished draft must not be exported."
                else:
                    approval["approved_by"] = "Personal Name <person@example.invalid>"
                self.save_manifest()
                self.pub("--check", MANIFEST, expect_success=False)
                self.pub("review", MANIFEST, expect_success=False)

    def test_export_refuses_source_tree_and_existing_destination(self):
        self.approve()
        existing = self.area / "existing"
        existing.mkdir()
        self.write("keep.txt", "Preserve source data.\n")
        for destination in (self.root / "export", self.root, existing):
            with self.subTest(output=str(destination)):
                self.pub("export", MANIFEST, "--output", str(destination), expect_success=False)
        self.assertEqual((self.root / "keep.txt").read_text(), "Preserve source data.\n")
        self.assertEqual(list(existing.iterdir()), [])

    def test_whole_repository_check_denies_unknown_files_and_draft_units(self):
        self.approve()
        self.pub("--check", "--target", "git")
        self.write("working-note.md", "Work in progress.\n")
        self.git("add", "--", "working-note.md")
        self.pub("--check", "--target", "git", expect_success=False)
        self.git("rm", "--cached", "--", "working-note.md")
        second = "demos/unfinished/project.yaml"
        self.write(second, json.dumps({"id": "unfinished", "status": "prototype",
                                     "publication": {"state": "draft"}}) + "\n")
        self.git("add", "--", second)
        self.pub("--check", "--target", "git", expect_success=False)

    def test_installed_hook_stops_real_push_before_remote_receives_wip(self):
        self.manifest["status"] = "prototype"
        self.manifest["publication"]["state"] = "draft"
        self.save_manifest()
        self.commit("Unfinished source must remain private")
        destination = self.bare_remote()
        self.install_hook_fixture()
        self.pub("install-hook")  # Reinstalling our own hook is safe and idempotent.
        result = self.git("push", "public", "HEAD:refs/heads/main", expect_success=False)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        refs = self.git("for-each-ref", "--format=%(refname)", cwd=destination)
        self.assertEqual(refs.stdout.strip(), "")
        head = self.git("rev-parse", "HEAD").stdout.strip()
        object_lookup = self.git("cat-file", "-e", head, cwd=destination, expect_success=False)
        self.assertNotEqual(object_lookup.returncode, 0)

    def test_approved_tip_does_not_authorize_earlier_wip_history(self):
        self.manifest["status"] = "prototype"
        self.manifest["publication"]["state"] = "draft"
        self.save_manifest()
        self.commit("Private draft")
        self.manifest["status"] = "stable"
        self.manifest["publication"]["state"] = "ready"
        self.save_manifest()
        self.approve()
        self.commit("Approved current snapshot")
        self.pub("--check", "--target", "git")
        destination = self.bare_remote()
        self.install_hook_fixture()
        result = self.git("push", "public", "HEAD:refs/heads/main", expect_success=False)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.git("for-each-ref", "--format=%(refname)", cwd=destination).stdout.strip(), "")

    def test_private_backup_exception_requires_exact_remote_name_and_url(self):
        self.manifest["status"] = "prototype"
        self.manifest["publication"]["state"] = "draft"
        self.save_manifest()
        self.commit("Private backup fixture")
        destination = self.bare_remote("backup")
        other_destination = self.bare_remote("public")
        self.git("config", "--add", "publication.privateRemote", "backup")
        self.git("config", "publication.privateRemoteUrl.backup", str(destination))
        self.install_hook_fixture()
        head = self.git("rev-parse", "HEAD").stdout.strip()
        update = "HEAD {} refs/heads/main {}\n".format(head, "0" * 40)
        self.pub("guard-push", "public", str(destination), expect_success=False, input_text=update)
        self.pub("guard-push", "backup", str(other_destination), expect_success=False, input_text=update)
        self.git("push", "backup", "HEAD:refs/heads/main")
        self.assertEqual(self.git("rev-parse", "refs/heads/main", cwd=destination).stdout.strip(),
                         self.git("rev-parse", "HEAD").stdout.strip())

    def test_global_and_runtime_config_cannot_authorize_private_backup(self):
        destination = self.bare_remote("backup")
        global_config = self.area / "global-git-config"
        self.git("config", "--file", str(global_config), "--add", "publication.privateRemote", "backup")
        self.git("config", "--file", str(global_config), "publication.privateRemoteUrl.backup", str(destination))
        self.environment["GIT_CONFIG_GLOBAL"] = str(global_config)
        self.pub("guard-push", "backup", str(destination), expect_success=False)
        self.git("config", "--local", "--add", "publication.privateRemote", "backup")
        self.pub("guard-push", "backup", str(destination), expect_success=False)
        self.git("config", "--local", "--unset-all", "publication.privateRemote")
        self.environment["GIT_CONFIG_GLOBAL"] = os.devnull
        self.environment.update({
            "GIT_CONFIG_COUNT": "2",
            "GIT_CONFIG_KEY_0": "publication.privateRemote",
            "GIT_CONFIG_VALUE_0": "backup",
            "GIT_CONFIG_KEY_1": "publication.privateRemoteUrl.backup",
            "GIT_CONFIG_VALUE_1": str(destination),
        })
        self.pub("guard-push", "backup", str(destination), expect_success=False)

    def test_hook_installer_preserves_existing_hook_and_custom_hooks_path(self):
        hook = self.root / ".git" / "hooks" / "pre-push"
        contents = "#!/bin/sh\nprintf 'existing hook remains intact\\n'\n"
        hook.write_text(contents, encoding="utf-8")
        hook.chmod(0o755)
        self.pub("install-hook", expect_success=False)
        self.assertEqual(hook.read_text(encoding="utf-8"), contents)
        hook.unlink()
        self.git("config", "core.hooksPath", "custom-hooks")
        self.pub("install-hook", expect_success=False)
        self.assertEqual(self.git("config", "--get", "core.hooksPath").stdout.strip(), "custom-hooks")


if __name__ == "__main__":
    unittest.main()
