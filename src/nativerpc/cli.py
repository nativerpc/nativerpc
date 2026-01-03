##
#   Native RPC Cli
#
#       main
##
import os
import sys


def main():
    dir = os.path.dirname(__file__)
    args = " ".join([f"\"{x}\"" for x in sys.argv[1:]])
    res = os.system(f"node {dir}/cli.js {args}")
    sys.exit(res)