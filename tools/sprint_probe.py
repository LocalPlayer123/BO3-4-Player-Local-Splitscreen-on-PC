"""Read-only Linux probe for the EZZ BOIII 3.0 / mod 2.6.6 sprint bug.

Reads /proc/PID/mem; never attaches, pauses, calls into, or writes to the game.
Run snapshot first, then watch while exercising the four controllers.
Addresses below are for the unpacked game with PE checksum 0x06531394.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import time


def find_game():
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            for line in proc.joinpath('maps').read_text().splitlines():
                fields = line.split()
                if line.endswith('/BlackOps3.exe') and fields[2] == '00000000':
                    return int(proc.name), int(fields[0].split('-')[0], 16)
        except (OSError, PermissionError):
            continue
    raise RuntimeError('No running BOIII game image found')


class Game:
    def __init__(self):
        self.pid, self.base = find_game()
        self.fd = os.open(f'/proc/{self.pid}/mem', os.O_RDONLY)
        pe = self.u32(self.base + 0x3c)
        if self.u32(self.base + pe + 24 + 64) != 0x06531394:
            raise RuntimeError('Unsupported game checksum; probe refused')
        self.keys = self.rip_target(0x013478b4)
        self.kbuttons = self.rip_target(0x012f719b)
        self.pads = self.rip_target(0x02285723)

    def read(self, address, length):
        value = os.pread(self.fd, length, address)
        if len(value) != length:
            raise RuntimeError(f'Short read at {address:#x}')
        return value

    def u32(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]

    def u64(self, address):
        return struct.unpack('<Q', self.read(address, 8))[0]

    def rip_target(self, rva):
        code = self.read(self.base + rva, 7)
        if code[:2] not in (b'\x48\x8d', b'\x4c\x8d') or code[2] & 0xc7 != 5:
            raise RuntimeError(f'Expected RIP-relative LEA at {rva:#x}')
        return self.base + rva + 7 + struct.unpack_from('<i', code, 3)[0]

    def cstring(self, address):
        if not address:
            return None
        try:
            return self.read(address, 128).split(b'\0', 1)[0].decode('ascii', errors='replace')
        except OSError:
            return '<unreadable>'

    def bindings(self, lc):
        row = self.read(self.keys + lc * 0x1940 + 0x138, 32 * 0x18)
        return [{
            'key': key,
            'down': struct.unpack_from('<I', row, key * 24)[0],
            'actions': list(struct.unpack_from('<II', row, key * 24 + 8)),
            'text': self.cstring(struct.unpack_from('<Q', row, key * 24 + 16)[0]),
        } for key in range(1, 32)]

    def stick_click_settings(self):
        """Read root DDL members; resolve offsets from the live definition."""
        storage = self.rip_target(0x02219fa7)
        names = {self.cstring(self.u64(self.base + 0x032bf430 + i * 8))
                 for i in (16, 17)}
        result = []
        for controller in range(4):
            settings = {'controller': controller}
            for slot in range(64):
                entry = storage + controller * 0x8958 + 0x10 + slot * 0x220
                props = self.u64(entry)
                if not props or self.u32(props) != 0 or self.u32(entry + 8) != 0:
                    continue
                context = entry + 0x18
                buffer, length, definition = (self.u64(context), self.u32(context + 8),
                                               self.u64(context + 16))
                if not buffer or not definition:
                    break
                root = self.u64(definition + 32)
                members, count = self.u64(root + 16), self.u32(root + 12)
                if count > 1024:
                    raise RuntimeError('Unexpected DDL member count')
                header_bits = self.u32(definition + 72)
                for index in range(count):
                    member = members + index * 72
                    name = self.cstring(self.u64(member))
                    if name not in names:
                        continue
                    bit_offset = header_bits + self.u32(member + 32)
                    if self.u32(member + 24) != 32 or bit_offset % 8 or bit_offset // 8 + 4 > length:
                        raise RuntimeError('Unexpected stick threshold DDL layout')
                    settings[name] = struct.unpack('<f', self.read(buffer + bit_offset // 8, 4))[0]
                break
            result.append(settings)
        return result

    def state(self):
        state = []
        for lc in range(4):
            pad = self.read(self.pads + lc * 0x70, 0x30)
            keys = self.read(self.keys + lc * 0x1940 + 0x138, 32 * 0x18)
            kb = self.read(self.kbuttons + lc * 0x498, 0x498)
            state.append({
                'lc': lc,
                'device': struct.unpack_from('<i', pad, 4)[0],
                'pad_bits': self.u32(self.pads + lc * 0x70 + 8),
                'stick_axes': [round(value, 3) for value in struct.unpack_from('<ffff', pad, 0x20)],
                'keys_down': [key for key in range(1, 32)
                              if struct.unpack_from('<I', keys, key * 24)[0]],
                'active_kbuttons': [i for i in range(49)
                                    if kb[i * 24 + 16]],
            })
        return state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--watch', type=float, default=0, help='seconds to record changes')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    game = Game()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    header = {
        'pid': game.pid, 'base': hex(game.base),
        'keys': hex(game.keys), 'kbuttons': hex(game.kbuttons), 'pads': hex(game.pads),
        'gamepad_translation': [list(struct.unpack('<II', game.read(game.base + 0x033c4240 + i * 8, 8)))
                               for i in range(16)],
        'bindings': [game.bindings(lc) for lc in range(4)],
        'stick_click_settings': game.stick_click_settings(),
    }
    with args.output.open('w') as output:
        output.write(json.dumps({'snapshot': header}) + '\n')
        print('SNAPSHOT', json.dumps(header), flush=True)
        previous = None
        end = time.monotonic() + args.watch
        while True:
            state = game.state()
            if state != previous:
                record = {'time': time.time(), 'state': state}
                output.write(json.dumps(record) + '\n')
                output.flush()
                print(json.dumps(record), flush=True)
                previous = state
            if time.monotonic() >= end:
                break
            time.sleep(0.01)
    os.close(game.fd)


if __name__ == '__main__':
    main()
