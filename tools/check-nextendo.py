#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused offline checks. Run in the assigned private Docker volume, not a full emulator build."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import subprocess
import tempfile
import threading


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, help="Optional pinned Eden source for derivative guards")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="prosperoeden-nextendo-") as tmp:
        out = Path(tmp)
        if args.source:
            guards = out / "guards.cmake"
            guards.write_text(f'''include("{root / 'headless/nextendo.cmake'}")
include("{root / 'headless/nextendo_native.cmake'}")
file(READ "${{NEXTENDO_SOURCE}}/src/core/internal_network/network.cpp" native)
nextendo_native_network(native)
file(WRITE "${{NEXTENDO_OUTPUT}}/native-network.cpp" "${{native}}")
''')
            subprocess.run(["cmake", f"-DNEXTENDO_SOURCE={args.source.resolve()}",
                            f"-DNEXTENDO_OUTPUT={out / 'derived'}", "-P",
                            str(guards)], check=True)
            generated = (out / "derived/acc.cpp").read_text()
            assert generated.count("Eden::Nextendo::GuestPidForUser") == 3
            assert generated.count("if (Eden::Nextendo::GuestEnabled())") >= 6
            tls = (out / "derived/ssl_backend_openssl.cpp").read_text()
            assert "Eden::Nextendo::EnforceGuestTls(ssl)" in tls
            assert "if (Eden::Nextendo::GuestEnabled()) return;" in tls
            dns = (out / "derived/sfdnsres.cpp").read_text()
            assert dns.count("Eden::Nextendo::GuestHost(") == 2
            native = (out / "derived/native-network.cpp").read_text()
            assert "lookup.Resolve(host.c_str()" in native and "lookup.Free(addrinfo)" in native
            print("Pinned acc/DNS/TLS checked derivatives: PASS")
        executable = out / "nextendo-check"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++20", "-O1", "-g", "-pthread",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror",
                        "-I", str(root / "headless"), str(root / "headless/nextendo.cpp"),
                        str(root / "headless/nextendo_http.cpp"), str(root / "headless/nextendo_check.cpp"),
                        "-lssl", "-lcrypto", "-o", str(executable)], check=True)
        result = subprocess.run([str(executable), str(out)], capture_output=True, text=True)
        if result.returncode:
            print(result.stdout, result.stderr)
            result.check_returncode()
        assert not result.stderr, result.stderr
        assert "nx2.synthetic" not in result.stdout
        assert "9223372036854775809" not in result.stdout
        print(result.stdout, end="")
        services = out / "services-check"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++20", "-O1", "-g", "-pthread",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror",
                        "-I", str(root / "headless"), str(root / "headless/nextendo.cpp"),
                        str(root / "headless/nextendo_services_check.cpp"), "-o", str(services)], check=True)
        service_data = out / "service-data"
        service_data.mkdir()
        subprocess.run([str(services), str(service_data)], check=True)
        print("Actual service adapter: captured profile, busy/storage/room guards and isolated explicit account check: PASS")
        transport = out / "transport-check"
        subprocess.run([os.environ.get("CC", "clang"), "-std=c11", "-DREMOTE_HTTP_HOST", "-Wall", "-Wextra", "-Werror",
                        "-I", str(root / "headless"), str(root / "headless/nextendo_transport_check.c"),
                        str(root / "headless/remote/http.c"), "-lcurl", "-pthread", "-o", str(transport)], check=True)
        calls = []

        class Server(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_GET(self):
                calls.append(self.path)
                expected = "Bearer " + "x" * 8192 if self.path == "/long" else "Bearer nextendo-synthetic-local-test"
                assert self.headers.get("Authorization") == expected
                if self.path == "/redirect":
                    self.send_response(302)
                    self.send_header("Location", "/final")
                else:
                    self.send_response(200)
                self.send_header("Content-Length", "0")
                self.end_headers()

        server = ThreadingHTTPServer(("127.0.0.1", 0), Server)
        worker = threading.Thread(target=server.serve_forever)
        worker.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/redirect"
            subprocess.run([str(transport), url, "1", "302"], check=True)
            assert calls == ["/redirect"]
            calls.clear()
            subprocess.run([str(transport), url, "0", "200"], check=True)
            assert calls == ["/redirect", "/final"]
            calls.clear()
            subprocess.run([str(transport), f"http://127.0.0.1:{server.server_port}/long", "1", "200", "long"], check=True)
            assert calls == ["/long"]
        finally:
            server.shutdown()
            worker.join()
            server.server_close()
        print("Real curl transport: full-length bearer preserved, credential-bound redirects refused; existing default redirects preserved: PASS")
        ca = out / "synthetic-ca.pem"
        subprocess.run(["openssl", "req", "-new", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=Nextendo synthetic test only", "-keyout", str(out / "synthetic-key.pem"),
                        "-out", str(ca)], check=True, capture_output=True)
        for missing in (False, True):
            native = out / ("native-missing" if missing else "native-present")
            subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror",
                            "-DEDEN_NEXTENDO_NATIVE=1", *(["-DNEXTENDO_NO_BRIDGE=1"] if missing else []),
                            "-I", str(root / "headless"), str(root / "headless/nextendo_native_check.cpp"),
                            "-lssl", "-lcrypto", "-o", str(native)], check=True)
            subprocess.run([str(native), str(ca)], check=True)
        print("PS5 adapter contract: native resolver/free pairing and CA import; absent adapter or trust file fails closed: PASS")


if __name__ == "__main__":
    main()
