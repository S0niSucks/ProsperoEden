#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual loopback browser callback + HTTPS OAuth/PKCE/token/storage. Synthetic provider only."""
import base64
import argparse
import hashlib
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading
import time
from urllib.parse import parse_qs, urlencode, urlsplit


def main():
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="nextendo-oauth-") as tmp:
        out = Path(tmp)
        compiler = os.environ.get("CXX", "clang++")
        http_object = out / "http.o"
        subprocess.run([os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-I", str(root / "headless/update_check"),
                        "-c", str(root / "headless/remote/http.c"), "-o", str(http_object)], check=True)
        binary = out / "oauth-check"
        subprocess.run([compiler, "-std=c++20", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pthread",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(root / "headless"),
                        str(root / "headless/nextendo.cpp"), str(root / "headless/nextendo_oauth.cpp"),
                        str(root / "headless/nextendo_http.cpp"),
                        str(root / "headless/nextendo_oauth_check.cpp"), str(http_object),
                        "-Wl,--wrap=curl_slist_append", "-Wl,--wrap=fsync",
                        "-lssl", "-lcrypto", "-lcurl", "-o", str(binary)], check=True)
        for name, host in (("good", "nextendo.network"), ("bad", "wrong.nextendo.network")):
            subprocess.run(["openssl", "req", "-new", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", f"/CN={host}", "-addext", f"subjectAltName=DNS:{host}",
                            "-keyout", str(out / f"{name}.key"), "-out", str(out / f"{name}.pem")],
                           check=True, capture_output=True)
        state = {"mode": "", "challenge": "", "posts": 0, "gets": 0, "failure": None}
        token_started = threading.Event()

        class Server(ThreadingHTTPServer):
            daemon_threads = True

            def handle_error(self, *_):
                state["failure"] = "synthetic provider handler failed"

        class Provider(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass  # never log request URLs, authorization codes, headers or token bodies

            def reply(self, status, body=b"", location=None):
                self.send_response(status)
                self.send_header("Content-Length", str(len(body)))
                if location:
                    self.send_header("Location", location)
                self.end_headers()
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                    pass  # expected for bounded/cancelled responses

            def do_GET(self):
                state["gets"] += 1
                url = urlsplit(self.path)
                if url.path == "/api/profile":
                    assert self.headers["X-Nextendo-Client-Id"] == "synthetic+registered&client"
                    if self.headers.get("Authorization") != "Bearer old-synthetic-local-session":
                        return self.reply(401, b'{"error":"missing bearer"}')
                    assert state["mode"] == "profile-duplicate"
                    return self.reply(200, b'{"username":null,"username":"synthetic"}')
                assert url.path == "/api/oauth/authorize"
                params = parse_qs(url.query, strict_parsing=True)
                assert params["response_type"] == ["code"] and params["scope"] == ["identity"]
                assert params["client_id"] == ["synthetic+registered&client"]
                assert params["app"] == ["synthetic-prosperoeden"]
                assert params["code_challenge_method"] == ["S256"]
                assert len(params["state"][0]) == 43 and len(params["code_challenge"][0]) == 43
                state["challenge"] = params["code_challenge"][0]
                state["redirect"] = params["redirect_uri"][0]
                callback = state["redirect"] + "?" + urlencode({"code": "synthetic-code-only", "state": params["state"][0]})
                self.reply(302, location=callback)

            def do_POST(self):
                state["posts"] += 1
                assert self.path == "/api/oauth/token"
                assert self.headers["X-Nextendo-Client-Id"] == "synthetic+registered&client"
                assert self.headers["Content-Type"] == "application/x-www-form-urlencoded"
                assert not self.headers.get("Authorization")
                size = int(self.headers["Content-Length"])
                assert 0 < size < 8192
                form = parse_qs(self.rfile.read(size).decode(), strict_parsing=True)
                assert form["grant_type"] == ["authorization_code"]
                assert form["code"] == ["synthetic-code-only"]
                assert form["client_id"] == ["synthetic+registered&client"]
                assert form["redirect_uri"] == [state["redirect"]]
                verifier = form["code_verifier"][0]
                assert len(verifier) == 43 and all(c.isalnum() or c in "-._~" for c in verifier)
                proof = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
                assert proof == state["challenge"]
                mode = state["mode"]
                token_started.set()
                if mode in ("cancel-token", "expiry-token"):
                    time.sleep(1)
                if mode == "redirect":
                    return self.reply(302, location="https://nextendo.network/must-not-follow")
                if mode == "oversize":
                    return self.reply(200, b"x" * 32769)
                if mode == "malformed":
                    return self.reply(200, b'{"account":{"pid":-1},"nex_token":"nx2.synthetic-proof-not-a-real-session"}')
                if mode == "provider-error":
                    return self.reply(401, b'{"error":"synthetic-code-only nx2.synthetic-proof-not-a-real-session"}')
                if mode == "duplicate-token":
                    return self.reply(200, b'{"account":{"pid":9},"nex_token":null,"nex_token":"nx2.synthetic-proof-not-a-real-session"}')
                if mode == "duplicate-account":
                    return self.reply(200, b'{"account":{"pid":0},"account":{"pid":9},"nex_token":"nx2.synthetic-proof-not-a-real-session"}')
                if mode == "duplicate-pid":
                    return self.reply(200, b'{"account":{"pid":0,"pid":9},"nex_token":"nx2.synthetic-proof-not-a-real-session"}')
                if mode == "escaped-duplicate-token":
                    return self.reply(200, b'{"account":{"pid":9},"nex_token":null,"nex_\\u0074oken":"nx2.synthetic-proof-not-a-real-session"}')
                self.reply(200, json.dumps({"account": {"pid": 9, "username": "discard-me", "friend_code": "discard-me"},
                                           "nex_token": "nx2.synthetic-proof-not-a-real-session"}).encode())

        servers = []
        for name in ("good", "bad"):
            server = Server(("127.0.0.1", 0), Provider)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(out / f"{name}.pem", out / f"{name}.key")
            server.socket = context.wrap_socket(server.socket, server_side=True)
            worker = threading.Thread(target=server.serve_forever)
            worker.start()
            servers.append((server, worker))
        trusted_browser = ssl.create_default_context(cafile=str(out / "good.pem"))

        def local(url, headers=None):
            parsed = urlsplit(url)
            assert parsed.scheme == "http" and parsed.hostname == "127.0.0.1"
            conn = http.client.HTTPConnection(parsed.hostname, parsed.port, timeout=7)
            conn.request("GET", parsed.path + ("?" + parsed.query if parsed.query else ""), headers=headers or {})
            response = conn.getresponse()
            result = response.status, dict(response.getheaders()), response.read()
            conn.close()
            return result

        def authorize(url):
            parsed = urlsplit(url)
            assert parsed.scheme == "https" and parsed.netloc == "nextendo.network"
            sock = socket.create_connection(("127.0.0.1", servers[0][0].server_port))
            with trusted_browser.wrap_socket(sock, server_hostname="nextendo.network") as tls:
                tls.sendall((f"GET {parsed.path}?{parsed.query} HTTP/1.1\r\nHost: nextendo.network\r\nConnection: close\r\n\r\n").encode())
                reply = http.client.HTTPResponse(tls)
                reply.begin()
                assert reply.status == 302
                callback = reply.getheader("Location")
                reply.read()
                return callback

        modes = ("success", "highlevel", "fixed", "recover", "slow-client", "declined", "expiry", "cancel-wait",
                 "cancel-token", "redirect", "oversize", "malformed", "provider-error", "unknown-ca",
                 "wrong-host", "storage-failure", "cancel-before-save", "cancel-in-save", "flood",
                 "duplicate-token", "duplicate-account", "duplicate-pid", "escaped-duplicate-token",
                 "profile-duplicate", "profile-header-oom", "profile-auth-oom", "profile-client-oom",
                 "token-header-oom", "expiry-token")
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--case", action="append", choices=modes, help="Run only selected cases")
        args = parser.parse_args()
        seen_states = set()
        try:
            for mode in args.case or modes:
                state.update(mode=mode, posts=0, gets=0, failure=None)
                token_started.clear()
                data = out / mode
                data.mkdir()
                token_port = servers[1 if mode == "wrong-host" else 0][0].server_port
                ca = out / ("bad.pem" if mode in ("wrong-host", "unknown-ca") else "good.pem")
                process = subprocess.Popen([str(binary), str(data / "nextendo"), str(token_port), str(ca), mode],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                try:
                    ready = process.stdout.readline().strip()
                    if mode.startswith("profile-"):
                        assert ready == "PROFILE", (mode, ready)
                        stdout, stderr = process.communicate(timeout=12)
                        assert process.returncode == 0 and not stderr, (mode, stderr)
                        assert stdout.strip() == f"DONE {mode} PASS", mode
                        assert state["failure"] is None and state["posts"] == 0
                        assert state["gets"] == (0 if mode.endswith("-oom") else 1)
                        print(f"OAuth regression {mode}: PASS")
                        continue
                    assert ready.startswith("START http://127.0.0.1:"), mode
                    start = ready[6:]
                    if mode == "cancel-wait":
                        process.stdin.write("cancel\n"); process.stdin.flush()
                    elif mode != "cancel-token":
                        process.stdin.write("continue\n"); process.stdin.flush()
                    if mode == "slow-client":
                        address = urlsplit(start)
                        slow = socket.create_connection((address.hostname, address.port))
                        slow.sendall(b"GET /start HTTP/1.1\r\n")
                    if mode == "flood":
                        for _ in range(64):
                            assert local(start + "?invalid=1")[0] == 400
                    elif mode not in ("cancel-wait", "expiry"):
                        status, headers, _ = local(start)
                        assert status == 303
                        auth = urlsplit(headers["Location"])
                        query = parse_qs(auth.query)
                        assert query["state"][0] not in seen_states
                        seen_states.add(query["state"][0])
                        callback = authorize(headers["Location"])
                        if mode == "recover":
                            callback_query = urlsplit(callback).query
                            base = callback.split("?")[0]
                            assert local(callback.replace(query["state"][0], "x" * 43))[0] == 400
                            assert local(callback + "&code=duplicate")[0] == 400
                            assert local(callback + "&error=access_denied")[0] == 400
                            assert local(callback, {"Host": "evil.example"})[0] == 400
                            assert local(callback, {"Origin": "https://evil.example"})[0] == 400
                            assert local(base + "?state=%GG&code=bad")[0] == 400
                            assert local(base + "?" + callback_query + "&state=duplicate")[0] == 400
                            assert local(callback + "&st%61te=duplicate")[0] == 400
                            assert local(callback + "&%63ode=duplicate")[0] == 400
                            assert local(callback.replace("synthetic-code-only", "%00"))[0] == 400
                            address = urlsplit(start)
                            for raw in (
                                f"GET /start HTTP/1.1\r\nHost: {address.netloc}\r\nHOST: {address.netloc}\r\n\r\n",
                                f"GET /start HTTP/1.1\r\nHost: {address.netloc}\r\nTransfer-Encoding: chunked\r\n\r\n",
                                f"GET /start HTTP/1.1\r\nHost: {address.netloc}\r\nContent-Length: 1\r\n\r\nx",
                                f"POST /start HTTP/1.1\r\nHost: {address.netloc}\r\n\r\n",
                                f"GET /start HTTP/1.0\r\nHost: {address.netloc}\r\n\r\n",
                            ):
                                with socket.create_connection((address.hostname, address.port), timeout=3) as bad:
                                    bad.sendall(raw.encode())
                                    reply = http.client.HTTPResponse(bad)
                                    reply.begin()
                                    assert reply.status == 400
                                    reply.read()
                            with socket.create_connection((address.hostname, address.port)) as bad:
                                bad.sendall(b"GET /start HTTP/1.1\r\nX-Large: " + b"x" * 9000)
                                bad.settimeout(3)
                                try:
                                    bad.recv(1024)
                                except ConnectionResetError:
                                    pass
                            assert state["posts"] == 0
                        if mode == "declined":
                            callback = callback.replace("code=synthetic-code-only", "error=access_denied")
                        assert local(callback)[0] == 200
                        if mode == "cancel-token":
                            assert token_started.wait(3)
                            process.stdin.write("cancel\n"); process.stdin.flush()
                    if mode == "slow-client":
                        slow.close()
                    stdout, stderr = process.communicate(timeout=12)
                    assert process.returncode == 0 and not stderr, (mode, stderr)
                    assert stdout.strip() == f"DONE {mode} PASS", mode
                    assert state["failure"] is None, mode
                    assert state["posts"] == (0 if mode in ("declined", "expiry", "cancel-wait", "unknown-ca", "wrong-host", "flood", "token-header-oom") else 1), mode
                    assert state["gets"] == (0 if mode in ("expiry", "cancel-wait", "flood") else 1), mode
                    print(f"OAuth regression {mode}: PASS")
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.communicate()
        finally:
            for server, worker in servers:
                server.shutdown()
                worker.join()
                server.server_close()
        print("OAuth actual loopback/HTTPS: S256 proof, state/host/parser bounds, expiry/cancel, one-use callback, registered client header, strict TLS/no redirects, private default-off replacement and failure preservation: PASS")


if __name__ == "__main__":
    main()
