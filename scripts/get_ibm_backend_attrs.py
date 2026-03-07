#! /usr/bin/env python3

'''
retrieves the attributes of an IBM backend and dumps them into a JSON file
'''

import argparse
import json

from qiskit_ibm_runtime import RuntimeEncoder, IBMBackend

from get_backend import get_fake_backend, get_real_backend, print_available_backends

import os

def save_jsons(backend_name: str, backend_obj: IBMBackend, output_dir: str) -> None:
    '''
    save the backend attributes to a JSON file
    the path is ~/.config/qsyn/cached_backend_attrs/<backend_name>_<timestamp>.json
    and ~/.config/qsyn/cached_backend_attrs/<backend_name>_properties_<timestamp>.json
    '''
    output_dir = os.path.expanduser(output_dir)
    # if the cached directory does not exist, create it
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # if cached files for this backend already exists, delete it
    if os.path.exists(os.path.expanduser(f"{output_dir}/{backend_name}_*.json")):
        os.remove(os.path.expanduser(f"{output_dir}/{backend_name}_*.json"))
    
    device_file = os.path.join(output_dir, f"{backend_name}.json")
    with open(device_file, "w") as f:
        json.dump(backend_obj.to_dict(), f, cls=RuntimeEncoder)
        
    props_file = os.path.join(output_dir, f"{backend_name}_properties.json")
    with open(props_file, "w") as f:
        json.dump(backend_obj.properties().to_dict(), f, cls=RuntimeEncoder)

def main() -> int:
    
    parser = argparse.ArgumentParser(
        description='Dump the attributes of an IBM backend',
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "backend",
        type=str,
        help='the name of the IBM backend to dump'
    )
    parser.add_argument(
        "-f", "--fake",
        action="store_true",
        help='use fake backend'
    )
    parser.add_argument(
        "-o", "--output",
        type=str,
        help='The output directory to save the backend attributes. Required unless --print-available-backends is specified. '
    )
    parser.add_argument(
        "--print-available-backends",
        action="store_true",
        help='print the available backends'
    )
    args = parser.parse_args()
    
    if args.print_available_backends:
        print_available_backends()
        return 0
    
    if args.backend is None:
        parser.print_help()
        return 1
    
    # normalize the backend name
    
    if not args.fake:
        backend_obj = get_real_backend(args.backend)
        if backend_obj is not None:
            save_jsons(args.backend, backend_obj, args.output)
        else:
            return 1
    else:
        backend_obj = get_fake_backend(args.backend)
        if backend_obj is not None:
            save_jsons(args.backend, backend_obj, args.output)
        else:
            return 1
    return 0

if __name__ == "__main__":
    exit(main())