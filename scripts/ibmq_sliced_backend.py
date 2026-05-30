#!/usr/bin/env python3
"""
Load IBM calibration JSON exported by ``qsyn device write --ibmq``.

The export is a single JSON file (``qsyn_ibmq_calibration`` format) containing
configuration, properties, and ``physical_qubits[logical]`` on the parent device.

Usage:
  from ibmq_sliced_backend import load_calibration, noise_model_from_calibration

  bundle = load_calibration("wip/torino_sub.json")
  noise = noise_model_from_calibration("wip/torino_sub.json")
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from qiskit_ibm_runtime.models.backend_configuration import BackendConfiguration
from qiskit_ibm_runtime.models.backend_properties import BackendProperties
from qiskit_aer.noise import NoiseModel

QSYN_CALIBRATION_KEY = "qsyn_ibmq_calibration"


@dataclass(frozen=True)
class SlicedBackendBundle:
    """Calibration bundle plus a minimal backend wrapper for Aer/Qiskit."""

    configuration: BackendConfiguration
    properties: BackendProperties
    physical_qubits: list[int]
    device_dict: dict[str, Any]
    properties_dict: dict[str, Any]
    source_path: Path

    @property
    def num_qubits(self) -> int:
        return self.configuration.n_qubits

    @property
    def name(self) -> str:
        return self.configuration.backend_name


class SlicedIBMBackend:
    """Minimal backend object sufficient for ``NoiseModel.from_backend``."""

    def __init__(self, bundle: SlicedBackendBundle):
        self._bundle = bundle

    @property
    def name(self) -> str:
        return self._bundle.name

    def configuration(self) -> BackendConfiguration:
        return self._bundle.configuration

    def properties(self) -> BackendProperties:
        return self._bundle.properties


def resolve_calibration_path(path: str | Path) -> Path:
    """
    Resolve a qsyn calibration export path to a readable JSON file.

    Accepts:
      - ``path/to/calibration.json`` — qsyn bundle (preferred)
      - ``path/to/name`` — adds ``.json`` if that file exists
      - ``path/to/dir/`` — sole ``*_remap.json`` legacy triple, or one ``qsyn_ibmq_calibration`` file
      - Legacy ``name`` stem under a directory (three-file export)
    """
    p = Path(path).expanduser()

    if p.is_file():
        if p.suffix == ".json":
            return p
        raise ValueError(f"Unrecognized calibration file: {p}")

    with_json = p.with_suffix(".json")
    if with_json.is_file():
        return with_json

    if p.is_dir():
        bundles = sorted(p.glob("*.json"))
        qsyn_bundles = []
        for candidate in bundles:
            try:
                data = json.loads(candidate.read_text())
            except json.JSONDecodeError:
                continue
            if QSYN_CALIBRATION_KEY in data:
                qsyn_bundles.append(candidate)
        if len(qsyn_bundles) == 1:
            return qsyn_bundles[0]
        if len(qsyn_bundles) > 1:
            raise FileNotFoundError(
                f"Multiple qsyn calibration files in {p}; pass the file path explicitly"
            )

        legacy = _resolve_legacy_triple(p, basename=None)
        if legacy is not None:
            return legacy

        raise FileNotFoundError(f"No qsyn calibration JSON found in {p}")

    legacy = _resolve_legacy_triple(p.parent, basename=p.name)
    if legacy is not None:
        return legacy

    if with_json.exists():
        return with_json

    raise FileNotFoundError(
        f"No calibration JSON found for {p} (expected {with_json} or legacy triple)"
    )


def _resolve_legacy_triple(directory: Path, basename: str | None) -> Path | None:
    """Return device JSON path for legacy three-file exports, if present."""
    root = directory
    if basename is None:
        remap_stems = {f.name[: -len("_remap.json")] for f in root.glob("*_remap.json")}
        if len(remap_stems) != 1:
            return None
        basename = next(iter(remap_stems))
    device_json = root / f"{basename}.json"
    properties_json = root / f"{basename}_properties.json"
    remap_json = root / f"{basename}_remap.json"
    if device_json.is_file() and properties_json.is_file() and remap_json.is_file():
        return device_json
    return None


def _ensure_qubit_frequencies(properties_dict: dict[str, Any], default_ghz: float = 5.0) -> None:
    """
    Cached/fake IBM properties often omit per-qubit ``frequency``.

    Aer ``NoiseModel.from_backend`` still requires it when T1/T2 are present.
    """
    for qubit_props in properties_dict.get("qubits", []):
        if not isinstance(qubit_props, list):
            continue
        names = {
            entry["name"]
            for entry in qubit_props
            if isinstance(entry, dict) and entry.get("name")
        }
        if "frequency" in names:
            continue
        date_value = next(
            (entry.get("date") for entry in qubit_props if isinstance(entry, dict) and entry.get("date")),
            None,
        )
        qubit_props.append(
            {
                "date": date_value,
                "name": "frequency",
                "unit": "GHz",
                "value": default_ghz,
            }
        )


def _bundle_from_parts(
    device_dict: dict[str, Any],
    properties_dict: dict[str, Any],
    physical_qubits: list[int],
    source_path: Path,
) -> SlicedBackendBundle:
    _ensure_qubit_frequencies(properties_dict)
    return SlicedBackendBundle(
        configuration=BackendConfiguration.from_dict(device_dict),
        properties=BackendProperties.from_dict(properties_dict),
        physical_qubits=physical_qubits,
        device_dict=device_dict,
        properties_dict=properties_dict,
        source_path=source_path,
    )


def _load_qsyn_bundle(data: dict[str, Any], source_path: Path) -> SlicedBackendBundle:
    return _bundle_from_parts(
        device_dict=data["configuration"],
        properties_dict=data["properties"],
        physical_qubits=[int(q) for q in data.get("physical_qubits", [])],
        source_path=source_path,
    )


def _load_legacy_triple(device_json: Path) -> SlicedBackendBundle:
    basename = device_json.stem
    root = device_json.parent
    properties_dict = json.loads((root / f"{basename}_properties.json").read_text())
    remap_dict = json.loads((root / f"{basename}_remap.json").read_text())
    device_dict = json.loads(device_json.read_text())
    return _bundle_from_parts(
        device_dict=device_dict,
        properties_dict=properties_dict,
        physical_qubits=[int(q) for q in remap_dict.get("physical_qubits", [])],
        source_path=device_json,
    )


def load_calibration(path: str | Path) -> SlicedBackendBundle:
    """Load a qsyn calibration export (single-file or legacy three-file)."""
    resolved = resolve_calibration_path(path)
    data = json.loads(resolved.read_text())
    if QSYN_CALIBRATION_KEY in data:
        return _load_qsyn_bundle(data, resolved)
    if _resolve_legacy_triple(resolved.parent, resolved.stem) == resolved:
        return _load_legacy_triple(resolved)
    raise ValueError(
        f"{resolved} is not a qsyn calibration bundle (missing {QSYN_CALIBRATION_KEY!r})"
    )


def load_sliced_backend(path: str | Path) -> SlicedIBMBackend:
    """Return a backend-like object built from a calibration export."""
    return SlicedIBMBackend(load_calibration(path))


def noise_model_from_calibration(
    path: str | Path,
    **noise_model_kwargs: Any,
) -> NoiseModel:
    """Build an Aer ``NoiseModel`` from a calibration export."""
    backend = load_sliced_backend(path)
    return NoiseModel.from_backend(backend, **noise_model_kwargs)


# Backward-compatible aliases
parse_calibration_export = resolve_calibration_path


def load_sliced_json_bundle(path: str | Path, basename: str | None = None) -> SlicedBackendBundle:
    if basename is not None:
        return load_calibration(Path(path) / basename)
    return load_calibration(path)


def noise_model_from_sliced(path: str | Path, basename: str | None = None, **kwargs: Any) -> NoiseModel:
    if basename is not None:
        return noise_model_from_calibration(Path(path) / basename, **kwargs)
    return noise_model_from_calibration(path, **kwargs)


def aer_simulator_from_sliced(
    path: str | Path,
    basename: str | None = None,
    method: str = "automatic",
    **noise_model_kwargs: Any,
):
    """Convenience helper: ``AerSimulator`` with calibration noise."""
    from qiskit_aer import AerSimulator

    if basename is not None:
        path = Path(path) / basename
    noise_model = noise_model_from_calibration(path, **noise_model_kwargs)
    return AerSimulator(method=method, noise_model=noise_model)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Inspect a qsyn IBM calibration export")
    parser.add_argument("path", help="Calibration .json file or directory")
    args = parser.parse_args()

    bundle = load_calibration(args.path)
    noise = noise_model_from_calibration(args.path)
    print(f"file: {bundle.source_path}")
    print(f"backend: {bundle.name}")
    print(f"qubits: {bundle.num_qubits}")
    print(f"physical_qubits (logical -> parent): {bundle.physical_qubits}")
    print(f"noise basis gates: {noise.basis_gates}")
