"""Real selected-host deadlines contain descendants, not unrelated processes."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))


def node():
    here, depth, mode = Path(sys.argv[-3]), int(sys.argv[-2]), sys.argv[-1]
    (here / f'{depth}.pid').write_text(str(os.getpid()))
    print(f'owned node {depth}', flush=True)
    if depth:
        subprocess.Popen([sys.executable, '-B', __file__, '--node', str(here), str(depth - 1), mode])
        if mode == 'escape':
            deadline = time.monotonic() + 4
            while not (here / '0.pid').exists() and time.monotonic() < deadline:
                time.sleep(.02)
            return
    time.sleep(60)


@unittest.skipUnless(os.name == 'nt', 'Windows Job containment contract')
class OwnedExecutionTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'tmp/world/end_to_end_validation/owned-host-tests'
        parent.mkdir(parents=True, exist_ok=True)
        directory = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(directory.cleanup)
        self.here = Path(directory.name)
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        self.kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]

    def probe(self, mode, kill_supervisor=False):
        from World.EndToEndValidation.app import execution, refinement_target
        errors, handles = [], []
        plain_run = execution._run
        def short_backstop(name, command, logs, timeout):
            return plain_run(name, command, logs, timeout=5)
        def execute():
            try:
                if kill_supervisor:
                    with patch.object(execution, '_run', side_effect=short_backstop):
                        refinement_target._run('tree', command, self.here, timeout=60)
                else:
                    refinement_target._run('tree', command, self.here, timeout=5)
            except Exception as error:
                errors.append(str(error))
        command = [sys.executable, '-B', __file__, '--node', str(self.here), '2', mode]
        sentinel = subprocess.Popen([sys.executable, '-B', '-c', 'import time; time.sleep(60)'])
        worker = threading.Thread(target=execute)
        try:
            worker.start()
            deadline = time.monotonic() + 4.5
            while len(list(self.here.glob('*.pid'))) != 3 and time.monotonic() < deadline:
                time.sleep(.02)
            for path in sorted(self.here.glob('*.pid')):
                handle = self.kernel.OpenProcess(0x100001, False, int(path.read_text()))
                # A fast intermediate parent may already be gone; the grandchild must exist.
                if handle:
                    handles.append(handle)
                elif path.stem == '0':
                    self.fail('grandchild disappeared before its owner deadline')
            self.assertEqual(3, len(list(self.here.glob('*.pid'))), 'complete tree was not started')
            worker.join(15)
            self.assertFalse(worker.is_alive(), 'owner exceeded deadline and drain grace')
            self.assertTrue(errors, 'live descendants or timeout were silently accepted')
            self.assertIsNone(sentinel.poll(), 'unrelated sentinel was terminated')
            for handle in handles:
                self.assertEqual(0, self.kernel.WaitForSingleObject(handle, 3000), 'owned descendant survived host completion')
        finally:
            for handle in handles:
                if self.kernel.WaitForSingleObject(handle, 0) == 258:
                    self.kernel.TerminateProcess(handle, 1)
                    self.kernel.WaitForSingleObject(handle, 3000)
                self.kernel.CloseHandle(handle)
            sentinel.terminate()
            sentinel.wait(timeout=5)

    def test_timeout_stops_root_child_and_grandchild(self):
        self.probe('timeout')

    def test_fast_intermediate_exit_cannot_leave_a_grandchild(self):
        self.probe('escape')

    def test_abrupt_supervisor_termination_closes_the_owned_job(self):
        self.probe('timeout', kill_supervisor=True)

    def test_normal_exit_retains_output_and_nonzero_exit_refuses(self):
        from World.EndToEndValidation.app import refinement_target
        for code in (0, 7):
            command = [sys.executable, '-B', '-c', f'print("retained output"); raise SystemExit({code})']
            if code:
                with self.assertRaises(Exception):
                    refinement_target._run('normal7', command, self.here, timeout=10)
            else:
                refinement_target._run('normal0', command, self.here, timeout=10)
                logs = '\n'.join(path.read_text(errors='replace') for path in self.here.glob('*.log'))
                self.assertIn('retained output', logs)


if __name__ == '__main__':
    if '--node' in sys.argv:
        node()
    else:
        unittest.main()
