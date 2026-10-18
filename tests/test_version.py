#!/usr/bin/env python3
"""
Version agreement across every manifest that ships geom-core.

pyproject.toml ([project] version) is the single source of truth:
- CMakeLists.txt derives PROJECT_VERSION from it (no literal allowed),
- setup.py must not restate it (setuptools reads pyproject.toml),
- package.json and src/typescript/index.ts cannot read a file at runtime,
  so they carry the same literal and this test keeps them equal.

Stdlib only; runs without building anything.
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEMVER = r"[0-9]+\.[0-9]+\.[0-9]+"


def _read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as fh:
        return fh.read()


def pyproject_version():
    # Same pattern CMakeLists.txt uses, so both parsers agree on the source.
    found = re.findall(r'^version = "(' + SEMVER + r')"$', _read("pyproject.toml"), re.M)
    assert len(found) == 1, f"expected one [project] version line, found {found}"
    return found[0]


def test_package_json_matches_pyproject():
    expected = pyproject_version()
    actual = json.loads(_read("package.json"))["version"]
    assert actual == expected, f"package.json {actual} != pyproject.toml {expected}"


def test_typescript_version_matches_pyproject():
    expected = pyproject_version()
    found = re.findall(
        r'^export const VERSION = "(' + SEMVER + r')";$',
        _read("src/typescript/index.ts"),
        re.M,
    )
    assert found == [expected], f"index.ts VERSION {found} != pyproject.toml {expected}"


def test_cmake_derives_version_from_pyproject():
    cmake = _read("CMakeLists.txt")
    assert "project(geom-core VERSION ${GEOM_CORE_VERSION}" in cmake
    assert '"${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml"' in cmake
    literal = re.findall(r"project\(geom-core VERSION " + SEMVER, cmake)
    assert not literal, f"CMakeLists.txt hard-codes a version: {literal}"


def test_no_other_hard_coded_version():
    assert not re.search(r"version\s*=\s*['\"]" + SEMVER, _read("setup.py")), (
        "setup.py restates the version; pyproject.toml is the source"
    )
    assert not re.search(r"geom-core v" + SEMVER, _read("src/cad/Engine.cpp")), (
        "Engine.cpp hard-codes a version; use GEOM_CORE_VERSION"
    )


def main():
    tests = [
        test_package_json_matches_pyproject,
        test_typescript_version_matches_pyproject,
        test_cmake_derives_version_from_pyproject,
        test_no_other_hard_coded_version,
    ]
    for test in tests:
        test()
        print(f"  ✓ {test.__name__}")
    print(f"✓ All manifests agree on version {pyproject_version()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
