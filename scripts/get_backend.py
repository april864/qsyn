#! /usr/bin/env python3

'''
retrieves the attributes of an IBM backend and dumps them into a JSON file
'''

import logging
import os
import sys

from dotenv import load_dotenv

# Silence QiskitRuntimeService warnings (instance selection, saved account, etc.)
logging.getLogger("qiskit_ibm_runtime.qiskit_runtime_service").setLevel(logging.ERROR)
from qiskit_ibm_runtime import IBMBackend, QiskitRuntimeService
import qiskit_ibm_runtime.fake_provider as fake_provider


def get_real_backend(backend_name: str) -> IBMBackend:
    load_dotenv()  # loads .env from current dir (or parent dirs)
    token = os.getenv("IBMQ_API_KEY")
    if not token:
        print("IBMQ_API_KEY not set in environment or .env", file=sys.stderr)
        return None
    service = QiskitRuntimeService(
        token=token,
    )
    try:
        backend = service.backend(backend_name)
        return backend
    except Exception:
        print(f"Error: Failed to get backend {backend_name}.", file=sys.stderr)
        print("Available backends:", file=sys.stderr)
        for b in service.backends():
            print(f"  {b.name}", file=sys.stderr)
        return None


def get_fake_backend(backend_name: str) -> IBMBackend:
    try:
        # Convert backend name to fake backend class name
        # e.g., "fake_manila" -> "FakeManilaV2" or "FakeManila"
        backend_parts = backend_name.replace("fake_", "").replace("_", " ").title().replace(" ", "")
        
        # Try V2 version first, then fall back to V1
        backend_class_name = f"Fake{backend_parts}V2"
        try:
            backend_class = getattr(fake_provider, backend_class_name)
            return backend_class()
        except AttributeError:
            # Try without V2 suffix
            backend_class_name = f"Fake{backend_parts}"
            backend_class = getattr(fake_provider, backend_class_name)
            return backend_class()
    except (AttributeError, Exception) as e:
        print(f"Error: Failed to get backend {backend_name}.")
        print(f"Please provide a valid fake backend name (e.g., fake_manila, fake_oslo)")
        print("\nAvailable fake backends can be found at:")
        print("https://docs.quantum.ibm.com/api/qiskit-ibm-runtime/fake_provider")
        return None