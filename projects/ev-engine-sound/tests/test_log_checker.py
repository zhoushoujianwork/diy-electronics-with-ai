#!/usr/bin/env python3
"""Synthetic diagnostic fixtures; these are not hardware test results."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

CHECKER=pathlib.Path(__file__).resolve().parents[1]/"tools/check_log.py"

class LogChecks(unittest.TestCase):
    def check(self, text):
        with tempfile.NamedTemporaryFile(mode="w",suffix=".log") as f:
            f.write(text); f.flush()
            return subprocess.run([sys.executable,str(CHECKER),f.name,"--min-heartbeats","3"],capture_output=True).returncode
    def healthy(self):
        return "\n".join(f"I ({i*1000}) engine: HEARTBEAT frames={i*32000} write_errors=0 render_max_us=1000 stack_audio=2048 stack_console=2048 stack_heartbeat=2048 fault=0" for i in range(1,4))
    def test_healthy(self): self.assertEqual(self.check(self.healthy()),0)
    def test_stack(self): self.assertNotEqual(self.check(self.healthy().replace("stack_audio=2048","stack_audio=512")),0)
    def test_fault(self): self.assertNotEqual(self.check(self.healthy().replace("fault=0","fault=1")),0)
    def test_deadline(self): self.assertNotEqual(self.check(self.healthy().replace("render_max_us=1000","render_max_us=9000")),0)
    def test_gap(self): self.assertNotEqual(self.check(self.healthy().replace("(3000)","(6000)")),0)
    def test_missing(self): self.assertNotEqual(self.check(""),0)
    def test_panic(self): self.assertNotEqual(self.check(self.healthy()+"\nGuru Meditation"),0)
    def test_stalled(self): self.assertNotEqual(self.check(self.healthy().replace("frames=96000","frames=64000")),0)

if __name__=="__main__": unittest.main()
