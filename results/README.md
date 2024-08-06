# Result from Event

These are the results from BSides Roanoke 2024 Badge Challenge.  In this
challege, players had an electronic badge with an PN532 NFC device.  Each badge
could pair, or find environmental NFC tags for points.  To score, players would
use their NFC Badge at a kiosk to upload their results for scoring.  This dump
of data is based on the uploaded results.

## badge\_data\_raw Folder

This folder contains raw data from the checkin from the players that checked in
at a badge kiosk.  Not all players checked in, so some data is likely missing.

Filenames are structured as:

    1720795265-08A08888.json
    \          \
     \          \- Badge ID, as read by Kiosk
      \
       \- Linux Epoch Time at time of API call.  For example, this dump was
          created at:
          Friday, July 12, 2024 10:41:05 AM GMT-04:00 DST

The structure of these files would contain JSON looking something like:

    {
      "badge_id": "08A04AE3",
      "hst": "309693A0D9AD3059FCA02BB830D7A030D7A030DA4F3025703056FDA06E5F3073AAA09719",
      "wt": "0001EB0006EC0008EA0009EB",
      "st": "08A0D99308A0D99308A07769"
    }

The `badge_id` is described in more detail below.

The `hst` or Hand Shake Table is the primary table to track handshakes.  It's 
composed of 3-byte badge\_ids (omitting `0x08`) either in it's normal form or
XOR'd form. There may be a bug somewhere in the kiosk or badge that alternates
normal and XOR'd form. There may be duplicates within the table, but only
uniques are counted for scoring.

The `wt` or Worm Table describes the path of each worm.  The first 2 bytes are
the index into the `hst` for which badge infected it. The last byte is the
value for the worm.  

Attendees would be white value if not associated with a worm.  When pairing
badges, a worm check would be performed to compare any associated worm values.
If an attendee did not have a worm value, they would assume their peer's worm.
Otherwise, the worm would follow a rock-paper-scissors game, where each value is
assigned rock, paper, or scissors.

Worm Values:

| Worm Value | LED Colors | Gets Beat By | Beats  | RPS Term |
| ---------- | ---------- | ------------ | ------ | -------- |
| `0xEA`     | Orange     | `0xEB`       | `0xEC` | Rock     |
| `0xEB`     | Blue       | `0xEC`       | `0xEA` | Paper    |
| `0xEC`     | Purple     | `0xEA`       | `0xEB` | Scissors |

The `st` is the Scan Table, to capture environmental reads of static NFC tags.
It should be referenced against the file `scanlist`.

If you use `sls/lib/part_handler.py`, you can decode some of a little easier.
It takes the `hst`, `wt`, and `st` as arguments. See example below.
```
$ python3 sls/lib/part_handler.py \
  "309693A0D9AD3059FCA02BB830D7A030D7A030DA4F3025703056FDA06E5F3073AAA09719" \
  "0001EB0006EC0008EA0009EB" "08A0D99308A0D99308A07769"

{
  "handshakes": [
    "A00D0C",
    "A0D9AD",
    "A0C263",
    "A02BB8",
    "A04C3F",
    "A04C3F",
    "A041D0",
    "A0BEEF",
    "A0CD62",
    "A06E5F",
    "A0E835",
    "A09719"
  ],
  "unique": 11,
  "packed_hst": "309693A0D9AD3059FCA02BB830D7A030D7A030DA4F3025703056FDA06E5F3073AAA09719",
  "encrypted": true
}
{
  "worms": [
    {
      "badge_ref": 1,
      "worm": "EB"
    },
    {
      "badge_ref": 6,
      "worm": "EC"
    },
    {
      "badge_ref": 8,
      "worm": "EA"
    },
    {
      "badge_ref": 9,
      "worm": "EB"
    }
  ],
  "current_worm": "EB",
  "packed_wt": "0001EB0006EC0008EA0009EB"
}
{
  "scans": [
    "08A0D993",
    "08A0D993",
    "08A07769"
  ],
  "failed_scans": [],
  "packed_st": "08A0D99308A0D99308A07769"
}
```

### Badge IDs

Badge IDs are the unique identifiers for each badge type. They compose of 3-4
bytes

_First Byte_ is being optional and is an artifact from the PN532 `0x08`. It's
only an artifact from the kiosk reading the badge.

_Second Byte_ defines the badge category, and is either `0xA0` for attendees or
or `0xB0` for sponsors.

_Third and fourth byte_ are the unique identifiers per badge.  While seemingly
small, there are 65536 possible badge IDs per category.  The attendee category
had 131 entries, giving a .2% chance of guessing badge ID correctly for
manipulation.  This should enable us to see audit activity if something were to
go sideways if someone figured out the API.

Badge IDs may be also seen as being XOR'd with the key of `0x909b9f`.

A full list of supported badges is in the file **badgelist**.


## badgelist

A list of all the badges used within the event.  Notable badges are:

* A00D0C - doc's badge.  Has the worm `0xEB`
* A08888 - koopatroopa's badge.  Has worm  `0xEC`
* A0BEEF - Organizer's badge.  Has worm `0xEA`
* A0DEAD - BenTheCyberOne's badge.  Has worm `0xEA`


## scanlist

A list of environmental NFC tags from the event.


## scores.json

The json file that fed the scoreboard on the website.  This had the resulting
score of each user that had scanned their badge. This file was regenerated upon
scoring.


## teams.json

The `teamId` to CTF's `teamName` json file.  Fetched from the CTF instance
periodically.

**Note:** Team names have been adjusted to remove names.  What was a name like
`Bob Smith` has been reduced to `BS`.


## secret\_scoreboard.csv

A CSV containing the highest scoring team member to Team Name.


## website/*

A dump of the static website.
