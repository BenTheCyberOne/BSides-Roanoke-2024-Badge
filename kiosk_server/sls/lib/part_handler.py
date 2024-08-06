#!/usr/bin/env python3
"""
Helper to decode part data

Works mainly in uppercase chars?

Cases:
"A0 DE AD A0 BE EF" "00 02 EA" "90 13 53 13 12 34 56 78 87 65 43 21"
"A0 DE AD A0 BE EF A0 DE AD" "00 02 EA 00 03 AB" "90 13 53 13 12 34 56 78 87 65 43 21"
"""

import json

hex_chars = 'abcdefABCDEF0123456789'
xor_key = [0x90, 0x9b, 0x9f]
valid_addr_magic = ["A0", "B0"]


def unxor(ehst, key=xor_key):
    """
    Given a string containing handshakes, xor decode them and return as string.

    Cyberchef playbook is similar to:
     * fromhex
     * xor
     * tohex

    Returns a string.
    """
    ret_string = []
    for i in range(0,len(ehst), 2):
        ret_string.append(f'{int(ehst[i:i+2],16)^key[int(i/2)%len(key)]:0>2x}')
    return ''.join(ret_string)


def decode_handshake_table(hst, badgelist=[]):
    """
    Decodes handshake part.

    Will test for:
     * If handshake needs to be decoded, based on first handshake
     * If handshakes start with 0xA0 or 0xB0
     * If handshakes are present in badge list, if a badge list is provided.
     * If the entirety of the list is properly formatted (3 bytes per badge)

    Provided badgelist must be uppercase

    Returns dict of one of the following:
     {"error": "error message"}
     {"handshakes": [<strings of 6 characters as hex handshakes>],
      "unique": int of unique handshake pairings
      "packed": uppercase original string, used for later}
    """
    if not isinstance(hst, str):
        return {"error": "not valid hst"}
    hst = "".join(hst.split())
    # error if invalid
    if len(hst) % 6 != 0 or any(c not in hex_chars for c in hst):
        print(f"'{hst}' was the hst")
        return {"error": "not valid hst"}

    # determine if decode is needed.
    # if key starts with
    encrypted = False
    parts = [hst[i:i+6] for i in range(0,len(hst), 6)]
    for i in range(len(parts)):
        part = parts[i]
        if part[0:2].upper() in decode_addrs:
            parts[i] = unxor(part).upper()
            encrypted = True
        elif part[0:2].upper() in valid_addr_magic:
            parts[i] = part.upper()
        else:
            return {"error": "not valid hst2"}

    # confirm all handshakes start with valid addresses
    if any(part[0:2] not in valid_addr_magic for part in parts):
        print(parts)
        return {"error": "not valid address"}

    if badgelist:
        if any(part not in badgelist for part in parts):
            return {"error": "not valid address2"}

    return {"handshakes": parts,
            "unique": len(set(parts)),
            "packed_hst": hst.upper(),
            "encrypted": encrypted}

def decode_worm_table(wt):
    """
    Decodes worm table and provides data

    Returns one of the following dicts:
    {"error": "error message"}
    {"worms": [{
            "badge_ref": int of badge that paired with me (starting with value 1)
            "worm_val": "hex of worm value"
            }]
     "current_worm": "Either empty string, or hex of what is valid"
     "packed_wt": uppercase string, used for later comparison
            }
    """
    # error if invalid
    if not isinstance(wt, str):
        return {"error": "not valid wt"}

    wt = "".join(wt.split())

    if len(wt) % 6 != 0 or any(c not in hex_chars for c in wt):
        return {"error": "not valid wt"}

    parts = []
    for i in range(0, len(wt), 6):
        parts.append({
                "badge_ref": int(wt[i:i+4], 16),
                "worm": wt[i+4:i+6]
                })
    return {"worms": parts,
            "current_worm": "" if not parts else parts[-1]['worm'],
            "packed_wt": wt.upper()}

def decode_scan_table(st, scanlist=[]):
    """
    Mainly breaks up a scan table into 4 byte chunks in hex

    Checks for:
     * If the entirety of the list is properly formatted (4 bytes per scan,
       no spaces)

    Returns a dict of containing key of scans
    """
    if not isinstance(st, str):
        return {"error": "not valid scan table"}

    st = "".join(st.split())

    if len(st) % 8 != 0 or any(c not in hex_chars for c in st):
        return {"error": "not valid scan table"}
    scans = []
    failed_scans = []
    for scan in [st[i:i+8] for i in range(0,len(st), 8)]:
        if scanlist and scan in scanlist:
            scans.append(scan)
        elif scanlist and not (scan in scanlist):
            failed_scans.append(scan)
        elif not scanlist:
            scans.append(scan)
    data = {'scans': scans,
            'failed_scans': failed_scans,
            "packed_st": st.upper()}
    return data


def main():
    if len(args.part) >= 1:
        print(json.dumps(decode_handshake_table("".join(args.part[0].split())),
                         indent=2))
    if len(args.part) >= 2:
        print(json.dumps(decode_worm_table("".join(args.part[1].split())),
                         indent=2))
    if len(args.part) >= 2:
        print(json.dumps(decode_scan_table("".join(args.part[2].split())),
                         indent=2))

decode_addrs = [unxor(addr) for addr in valid_addr_magic]

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=str(__doc__),
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("part",
                        nargs='+',
                        help="part of the badge")
    args = parser.parse_args()
    main()
