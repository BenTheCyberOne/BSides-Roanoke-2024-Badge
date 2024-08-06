#!/usr/bin/env python3
"""
Helper to test api

Cases:
"A0 DE AD A0 BE EF" "00 02 EA" "90 13 53 13 12 34 56 78 87 65 43 21"
"A0 DE AD A0 BE EF A0 DE AD" "00 02 EA 00 03 AB" "90 13 53 13 12 34 56 78 87 65 43 21"
"""

import requests

def main():
    if len(args.part) >= 1:
        hst = ''.join(args.part[0].split())
    else:
        print("using default hst")
        hst = "A0DEADA0BEEF"

    if len(args.part) >= 2:
        wt = ''.join(args.part[1].split())
    else:
        print("using default hst")
        wt = "0002EA"

    if len(args.part) >= 3:
        st = ''.join(args.part[2].split())
    else:
        print("using default st")
        st = "90135313"

    response = requests.post("https://2024-api.badge.bsidesroa.org/dump",
                             json={"badge_id": args.badge_id,
                                   "parts": [hst, wt, st]})
    print(response.text)

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=str(__doc__),
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("part",
                        nargs='*',
                        help="part of the badge")
    parser.add_argument("--badge-id",
                        default="A0F00D",
                        help="set badge id")
    args = parser.parse_args()
    main()
