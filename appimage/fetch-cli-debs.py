#!/usr/bin/env python3
"""
Query the ProtonVPN public apt repository and print download URLs for
proton-vpn-cli at the requested version and all of its ProtonVPN Python
dependencies (those not available on PyPI).

Usage:
    python3 fetch-cli-debs.py <cli_version> [deb_arch]

deb_arch is the Debian architecture of the native packages to fetch (amd64 or
arm64). It defaults to the architecture of the machine running the script.

Output: one HTTPS URL per line, suitable for wget.
"""

import platform
import re
import sys
import urllib.request

REPO_BASE = "https://repo.protonvpn.com/debian"

# Debian architecture names for the CPUs the AppImages are built for.
DEB_ARCH_FOR_MACHINE = {"x86_64": "amd64", "aarch64": "arm64"}

# ProtonVPN Python packages to pull from the apt repo (not on PyPI).
# Packages are resolved in this order; each one's .deb is output.
PROTON_PYTHON_DEPS = [
    "python3-proton-core",
    "python3-proton-keyring-linux",
    "python3-proton-keyring-linux-secretservice",
    "python3-proton-vpn-api-core",
    "python3-proton-vpn-session",
    "python3-proton-vpn-logger",
    "python3-proton-vpn-connection",
    # python3-proton-vpn-local-agent is intentionally absent: api-core >= 5.5
    # ships it built in (proton.vpn.platform.local_agent, inside its own
    # native platform.abi3.so) and declares Replaces: on the old package.
]


def fetch_text(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "Debian APT-HTTP/1.3"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read().decode()


def parse_packages(text: str) -> dict[str, list[dict]]:
    """Parse a Debian Packages file into {name: [entry, ...]}."""
    pkgs: dict[str, list[dict]] = {}
    for block in text.split("\n\n"):
        block = block.strip()
        if not block:
            continue
        entry: dict[str, str] = {}
        for line in block.split("\n"):
            if ":" in line:
                k, _, v = line.partition(":")
                entry[k.strip()] = v.strip()
        name = entry.get("Package", "")
        if name:
            pkgs.setdefault(name, []).append(entry)
    return pkgs


def version_key(v: str) -> tuple:
    """Coerce a Debian-style version into a sortable tuple."""
    return tuple(int(x) if x.isdigit() else x for x in re.split(r"[.\-~]", v))


def latest_satisfying(name: str, entries: list[dict], min_ver: str | None,
                      max_ver: str | None = None) -> dict:
    valid = entries
    if min_ver:
        mv = version_key(min_ver)
        valid = [e for e in valid if version_key(e.get("Version", "0")) >= mv]
    if max_ver:
        xv = version_key(max_ver)
        valid = [e for e in valid if version_key(e.get("Version", "0")) <= xv]
    if not valid:
        # Fail the build rather than bundle a version the CLI declares
        # incompatible: it would build cleanly and then crash at runtime.
        bounds = " and ".join(b for b in (min_ver and f">= {min_ver}",
                                          max_ver and f"<= {max_ver}") if b)
        newest = max(entries, key=lambda e: version_key(e.get("Version", "0")))
        sys.exit(f"ERROR: no {name} version satisfies {bounds} "
                 f"(newest in repo: {newest.get('Version', '?')})")
    return max(valid, key=lambda e: version_key(e.get("Version", "0")))


def deb_url(entry: dict) -> str:
    return f"{REPO_BASE}/{entry['Filename']}"


def main() -> None:
    if len(sys.argv) not in (2, 3):
        print(f"Usage: {sys.argv[0]} <cli_version> [deb_arch]", file=sys.stderr)
        sys.exit(1)

    cli_version = sys.argv[1]
    if len(sys.argv) == 3:
        deb_arch = sys.argv[2]
    else:
        deb_arch = DEB_ARCH_FOR_MACHINE.get(platform.machine(), "")
    if deb_arch not in DEB_ARCH_FOR_MACHINE.values():
        print(f"ERROR: unsupported architecture '{deb_arch or platform.machine()}' "
              f"(supported: {', '.join(DEB_ARCH_FOR_MACHINE.values())})", file=sys.stderr)
        sys.exit(1)

    all_pkgs  = parse_packages(fetch_text(f"{REPO_BASE}/dists/stable/main/binary-all/Packages"))
    arch_pkgs = parse_packages(fetch_text(f"{REPO_BASE}/dists/stable/main/binary-{deb_arch}/Packages"))

    # Find the exact CLI .deb
    cli_entries = [e for e in all_pkgs.get("proton-vpn-cli", [])
                   if e.get("Version") == cli_version]
    if not cli_entries:
        print(f"ERROR: proton-vpn-cli {cli_version} not found in repo", file=sys.stderr)
        sys.exit(1)
    cli_entry = cli_entries[0]
    print(deb_url(cli_entry))

    # Parse minimum version requirements from the CLI's Depends field
    min_versions: dict[str, str | None] = {}
    for dep in cli_entry.get("Depends", "").split(","):
        dep = dep.strip()
        m = re.match(r"([\w\-]+)\s*\(>=\s*([\d.]+)\)", dep)
        if m:
            min_versions[m.group(1)] = m.group(2)
        else:
            pkg_name = dep.split()[0] if dep else ""
            if pkg_name:
                min_versions.setdefault(pkg_name, None)

    # Resolve each ProtonVPN Python dependency
    for dep_name in PROTON_PYTHON_DEPS:
        # Search both indexes together: a package can move between them when
        # it gains native code (api-core went from binary-all to the
        # architecture-specific index at 5.5), leaving older versions behind
        # in binary-all.
        entries = all_pkgs.get(dep_name, []) + arch_pkgs.get(dep_name, [])
        if not entries:
            print(f"WARNING: {dep_name} not found in repo, skipping", file=sys.stderr)
            continue
        print(deb_url(latest_satisfying(dep_name, entries, min_versions.get(dep_name))))


if __name__ == "__main__":
    main()
