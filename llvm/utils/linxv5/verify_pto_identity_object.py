import json
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
assert data[:6] == b"\x7fELF\x02\x01"
section_offset = struct.unpack_from("<Q", data, 40)[0]
section_size, section_count, names_index = struct.unpack_from("<HHH", data, 58)
sections = [struct.unpack_from("<IIQQQQIIQQ", data,
                               section_offset + section_size * index)
            for index in range(section_count)]
names_header = sections[names_index]
names = data[names_header[4]:names_header[4] + names_header[5]]
notes = []
for section in sections:
    name = names[section[0]:].split(b"\0", 1)[0]
    if name != b".note.pto.isa":
        continue
    assert section[1] == 7 and section[2] & 2 and section[8] == 4
    contents = data[section[4]:section[4] + section[5]]
    owner_size, descriptor_size, note_type = struct.unpack_from("<III", contents)
    assert owner_size == 4 and note_type == 1
    assert contents[12:16] == b"PTO\0"
    descriptor = contents[16:16 + descriptor_size]
    expected = {
        "encoding_abi": "pto-isa-0.59.0-mode-function-v1",
        "encoding_projection_sha256": "4effea27acd51de4f637bddbee7d80e135c2d53775fa90e7d47c49041da8f693",
        "release": "0.59.0",
    }
    assert descriptor == json.dumps(expected, sort_keys=True,
                                    separators=(",", ":")).encode()
    assert not any(contents[16 + descriptor_size:])
    notes.append(descriptor)
assert len(notes) == 1
print("PTO ISA development identity verified")
