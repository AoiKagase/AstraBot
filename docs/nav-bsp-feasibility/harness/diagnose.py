"""Read-only GoldSrc NAV v5 / BSP v30 audit. No NAV writer, engine or optimizer.

All collision values are mathematical partitions of the compiled BSP, without
GoldSrc's trace epsilon, movement integration, entity spawning or game rules.
"""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys

SOLID = -2
PRIORITY = (860, 874, 1221, 1632, 1633, 1715)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Cursor:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def read(self, fmt):
        fmt = '<' + fmt
        size = struct.calcsize(fmt)
        if self.pos + size > len(self.data):
            raise ValueError(f'truncated NAV at {self.pos}')
        values = struct.unpack_from(fmt, self.data, self.pos)
        self.pos += size
        return values[0] if len(values) == 1 else values

    def string(self):
        length = self.read('H')
        if not length or self.pos + length > len(self.data):
            raise ValueError('invalid place name')
        value = self.data[self.pos:self.pos + length]
        self.pos += length
        if value[-1] != 0:
            raise ValueError('unterminated place name')
        return value[:-1].decode('latin1')


class Area:
    def __init__(self, identity, attr, geometry, links, place=0, hiding=(), approaches=(), encounters=()):
        self.id, self.attr, self.g, self.links = identity, attr, geometry, links
        self.place, self.hiding, self.approaches, self.encounters = place, hiding, approaches, encounters

    def z(self, x, y):
        x0, y0, z00, x1, y1, z11, z10, z01 = self.g
        if x1 == x0 or y1 == y0:
            return z00
        u = min(1, max(0, (x - x0) / (x1 - x0)))
        v = min(1, max(0, (y - y0) / (y1 - y0)))
        return (z00 + u * (z10 - z00)) * (1 - v) + (z01 + u * (z11 - z01)) * v

    def center(self):
        x, y = (self.g[0] + self.g[3]) / 2, (self.g[1] + self.g[4]) / 2
        return x, y, self.z(x, y)

    def contains(self, x, y):
        return self.g[0] <= x <= self.g[3] and self.g[1] <= y <= self.g[4]

    def digest(self):
        # Canonical typed semantic stream, independently matched by C++.
        h = 14695981039346656037

        def mix(value, floating=False):
            nonlocal h
            for byte in struct.pack('<f' if floating else '<I', value):
                h = ((h ^ byte) * 1099511628211) & 0xffffffffffffffff

        mix(self.id); mix(self.attr)
        for f in self.g:
            mix(f, True)
        mix(self.place)
        for ids in self.links:
            mix(len(ids))
            for identity in ids:
                mix(identity)
        mix(len(self.hiding))
        for identity, x, y, z, flags in self.hiding:
            mix(identity)
            for v in (x, y, z):
                mix(v, True)
            mix(flags)
        mix(len(self.approaches))
        for values in self.approaches:
            for v in values:
                mix(v)
        mix(len(self.encounters))
        for fr, fd, to, td, spots in self.encounters:
            for v in (fr, fd, to, td, len(spots)):
                mix(v)
            for identity, t in spots:
                mix(identity); mix(t)
        return f'{h:016x}'


class Nav:
    def __init__(self, data):
        c = Cursor(data)
        magic, self.version, self.bsp_size = c.read('III')
        if magic != 0xfeedface or self.version != 5:
            raise ValueError('this audit supports NAV v5 only')
        self.places = [c.string() for _ in range(c.read('H'))]
        count = c.read('I')
        if count == 0 or count > 65535:
            raise ValueError('invalid area count')
        self.areas = []
        for _ in range(count):
            identity, attr = c.read('IB')
            g = c.read('8f')
            links = [[c.read('I') for _ in range(c.read('I'))] for _ in range(4)]
            hiding = [c.read('I3fB') for _ in range(c.read('B'))]
            approaches = [c.read('IIBIB') for _ in range(c.read('B'))]
            encounters = []
            for _ in range(c.read('I')):
                fr, fd, to, td = c.read('IBIB')
                spots = [c.read('IB') for _ in range(c.read('B'))]
                encounters.append((fr, fd, to, td, spots))
            self.areas.append(Area(identity, attr, g, links, c.read('H'), hiding, approaches, encounters))
        if c.pos != len(data):
            raise ValueError(f'trailing NAV bytes: {len(data) - c.pos}')
        self.by_id = {a.id: a for a in self.areas}
        self.errors = []
        if len(self.by_id) != len(self.areas):
            self.errors.append('duplicate_area_id')
        spots = [s[0] for a in self.areas for s in a.hiding]
        if len(set(spots)) != len(spots):
            self.errors.append('duplicate_hiding_spot_id')
        spots = set(spots)
        for a in self.areas:
            if not all(math.isfinite(v) for v in a.g) or a.g[0] >= a.g[3] or a.g[1] >= a.g[4]:
                self.errors.append(f'invalid_geometry:{a.id}')
            if a.place > len(self.places):
                self.errors.append(f'invalid_place:{a.id}')
            for ids in a.links:
                if len(ids) != len(set(ids)):
                    self.errors.append(f'duplicate_connection:{a.id}')
                for target in ids:
                    if target not in self.by_id:
                        self.errors.append(f'missing_area:{a.id}:{target}')
            for here, prev, ph, nex, nh in a.approaches:
                for target in (here, prev, nex):
                    if target and target not in self.by_id:
                        self.errors.append(f'invalid_approach:{a.id}:{target}')
                if ph >= 8 or nh >= 8:
                    self.errors.append(f'invalid_traverse:{a.id}')
            for fr, fd, to, td, ss in a.encounters:
                if fr not in self.by_id or to not in self.by_id or fd >= 4 or td >= 4:
                    self.errors.append(f'invalid_encounter:{a.id}')
                if any(s not in spots for s, _ in ss):
                    self.errors.append(f'invalid_encounter_spot:{a.id}')
        self.grid = collections.defaultdict(list)
        for a in self.areas:
            for ix in range(math.floor(a.g[0] / 128), math.floor(a.g[3] / 128) + 1):
                for iy in range(math.floor(a.g[1] / 128), math.floor(a.g[4] / 128) + 1):
                    self.grid[ix, iy].append(a)

    def containing(self, x, y, z, tolerance=64):
        return [a for a in self.grid.get((math.floor(x / 128), math.floor(y / 128)), ())
                if a.contains(x, y) and abs(a.z(x, y) - z) <= tolerance]


def brush_policy(e):
    c = e.get('classname', '')
    if c == 'func_illusionary':
        return 'ignored_noncollision'
    if c.startswith('trigger_') or c in ('func_bomb_target', 'func_buyzone', 'func_hostage_rescue', 'func_escapezone', 'func_vip_safetyzone', 'func_tankcontrols'):
        return 'ignored_trigger'
    if c == 'func_ladder':
        return 'ladder'
    if c == 'func_breakable':
        return 'breakable_intact'
    if c in ('func_door', 'func_door_rotating', 'momentary_door'):
        if float(e.get('skin','0')) != 0 and c == 'func_door':
            return 'fluid_not_solid_floor'
        if int(e.get('spawnflags','0')) & 8 and c != 'momentary_door':
            return 'door_passable'
        return 'door_reference_pose'
    if c in ('func_wall', 'button_target', 'func_healthcharger', 'func_recharge'):
        return 'static_solid'
    if c == 'func_wall_toggle':
        return 'toggle_initially_off' if int(e.get('spawnflags', '0')) & 1 else 'toggle_reference_on'
    if c == 'func_rotating' and int(e.get('spawnflags','0')) & 64 or c == 'func_conveyor' and int(e.get('spawnflags','0')) & 2:
        return 'ignored_noncollision'
    if c in ('func_train', 'func_plat', 'func_rotating', 'func_pendulum', 'func_conveyor', 'func_vehicle', 'func_button', 'func_tank', 'func_tanklaser'):
        return 'movable_reference_pose'
    if c == 'func_water':
        return 'fluid_not_solid_floor'
    return 'unresolved_entity_policy'


def vadd(a, b):
    return tuple(x + y for x, y in zip(a, b))


def lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


class Bsp:
    FORMATS = {1: '4fi', 3: '3f', 5: 'i2h6h2H', 7: 'Hhihh4Bi',
               9: 'i2h', 10: 'ii6h2H4B', 12: '2H', 13: 'i', 14: '9f7i'}

    def __init__(self, data):
        if len(data) < 124 or struct.unpack_from('<i', data)[0] != 30:
            raise ValueError('not BSP v30')
        self.data = data
        self.lumps = [struct.unpack_from('<ii', data, 4 + 8 * i) for i in range(15)]
        for o, n in self.lumps:
            if o < 0 or n < 0 or o + n > len(data):
                raise ValueError('BSP lump outside file')
        records = {}
        for i, fmt in self.FORMATS.items():
            o, n = self.lumps[i]
            if n % struct.calcsize('<' + fmt):
                raise ValueError(f'misaligned BSP lump {i}')
            records[i] = list(struct.iter_unpack('<' + fmt, data[o:o + n]))
        self.planes, self.nodes, self.leaves = records[1], records[5], records[10]
        self.clips, self.models = records[9], records[14]
        self.vertices, self.faces = records[3], records[7]
        self.edges, self.surfedges = records[12], [v[0] for v in records[13]]
        self.validate_partitions()
        o, n = self.lumps[0]
        self.entities = [dict(re.findall(r'"([^"\n]*)"\s*"([^"\n]*)"', block))
                         for block in re.findall(r'\{([^{}]*)\}', data[o:o + n].decode('latin1'))]
        self.brushes = []
        self.policies = collections.Counter()
        for e in self.entities:
            if not e.get('model', '').startswith('*'):
                continue
            policy = brush_policy(e)
            self.policies[policy] += 1
            model = int(e['model'][1:])
            if not 0 <= model < len(self.models):
                raise ValueError('invalid entity brush model')
            origin = tuple(map(float, e.get('origin', '0 0 0').split()))
            angles = tuple(map(float, e.get('angles', '0 0 0').split()))
            if len(origin) != 3 or len(angles) != 3:
                raise ValueError('malformed brush transform')
            self.brushes.append((model, policy, origin, angles, e))
        self.solids = [(0, 'world', (0, 0, 0), (0, 0, 0), {})] + [
            item for item in self.brushes if item[1] in (
                'static_solid', 'breakable_intact', 'door_reference_pose',
                'toggle_reference_on', 'movable_reference_pose')]

    @classmethod
    def synthetic(cls, planes, nodes, contents, clips):
        obj = cls.__new__(cls)
        obj.planes, obj.nodes, obj.leaves, obj.clips = planes, nodes, [(v,) for v in contents], clips
        return obj

    def validate_partitions(self):
        for table, hull0 in ((self.nodes, True), (self.clips, False)):
            for nd in table:
                if not 0 <= nd[0] < len(self.planes):
                    raise ValueError('invalid plane index')
                for child in nd[1:3]:
                    if child >= len(table) or (hull0 and child < -len(self.leaves)):
                        raise ValueError('invalid BSP child')
            # Fail on cycles rather than hanging a malformed-input audit.
            done = set()
            for root in range(len(table)):
                stack, active = [(root, False)], set()
                while stack:
                    ni, leaving = stack.pop()
                    if ni < 0 or ni in done:
                        continue
                    if leaving:
                        active.remove(ni); done.add(ni)
                    else:
                        if ni in active:
                            raise ValueError('cyclic BSP partition')
                        active.add(ni); stack.append((ni, True))
                        stack.extend((child, False) for child in table[ni][1:3])
        for m in self.models:
            for h, head in enumerate(m[9:13]):
                table = self.nodes if h == 0 else self.clips
                if head >= len(table) or (h == 0 and head < -len(self.leaves)):
                    raise ValueError('invalid BSP headnode')
        if any(not all(math.isfinite(v) for v in plane[:4]) for plane in self.planes):
            raise ValueError('nonfinite BSP plane')

    def leaf(self, hull, ni):
        return self.leaves[-ni - 1][0] if hull == 0 else ni

    def contents(self, hull, head, point):
        table = self.nodes if hull == 0 else self.clips
        ni = head
        while ni >= 0:
            nd = table[ni]; plane = self.planes[nd[0]]
            ni = nd[1 if dot(point, plane[:3]) >= plane[3] else 2]
        return self.leaf(hull, ni)

    def trace(self, hull, head, start, end):
        """First solid interval (fraction, entry normal), exact plane split.

        Not SV_RecursiveHullCheck: no 1/32 epsilon, movement, slide or step.
        """
        table = self.nodes if hull == 0 else self.clips
        stack = [(head, 0., 1., (0., 0., 0.))]
        while stack:
            ni, ta, tb, normal = stack.pop()
            if ni < 0:
                if self.leaf(hull, ni) == SOLID:
                    return ta, normal
                continue
            nd = table[ni]; plane = self.planes[nd[0]]
            pa, pb = lerp(start, end, ta), lerp(start, end, tb)
            da, db = dot(pa, plane[:3]) - plane[3], dot(pb, plane[:3]) - plane[3]
            if da >= 0 and db >= 0:
                stack.append((nd[1], ta, tb, normal))
            elif da < 0 and db < 0:
                stack.append((nd[2], ta, tb, normal))
            else:
                tm = ta + (tb - ta) * da / (da - db)
                side = 1 if da >= 0 else 2
                entry = tuple(v * (1 if side == 1 else -1) for v in plane[:3])
                stack.append((nd[3 - side], tm, tb, entry))
                stack.append((nd[side], ta, tm, normal))
        return None

    @staticmethod
    def local(point, origin, angles):
        p = tuple(v - o for v, o in zip(point, origin))
        if not any(angles):
            return p
        pitch, yaw, roll = map(math.radians, angles)
        sp, cp, sy, cy, sr, cr = math.sin(pitch), math.cos(pitch), math.sin(yaw), math.cos(yaw), math.sin(roll), math.cos(roll)
        forward = (cp * cy, cp * sy, -sp)
        minus_right = (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp)
        up = (cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp)
        return dot(p, forward), dot(p, minus_right), dot(p, up)

    def candidates(self, start, end, hull, state):
        padxy, padz = (0, 0) if hull == 0 else (16, 36 if hull == 1 else 18)
        for model, policy, origin, angles, entity in self.solids:
            if state == 'removed_open' and policy in ('breakable_intact', 'door_reference_pose'):
                continue
            a, b = self.local(start, origin, angles), self.local(end, origin, angles)
            if model:
                m = self.models[model]
                if any(max(a[i], b[i]) < m[i] - (padz if i == 2 else padxy) - .01 or
                       min(a[i], b[i]) > m[i + 3] + (padz if i == 2 else padxy) + .01 for i in range(3)):
                    continue
            yield model, policy, a, b

    def scene_trace(self, start, end, hull=0, state='reference'):
        best = None
        for model, policy, a, b in self.candidates(start, end, hull, state):
            h = self.trace(hull, self.models[model][9 + hull], a, b)
            if h is not None and (best is None or h[0] < best['fraction']):
                normal = h[1]
                # Convert entry normals for rotated brush models to world axes.
                angles = next((v[3] for v in self.solids if v[0] == model), (0,0,0))
                if any(angles):
                    normal = tuple(dot(self.local(tuple(1 if j == i else 0 for j in range(3)), (0,0,0), angles), h[1]) for i in range(3))
                best = dict(fraction=h[0], normal=normal, model=model, policy=policy,
                            z=start[2] + (end[2] - start[2]) * h[0])
        return best

    def scene_solid(self, point, hull=1, state='reference'):
        return any(self.contents(hull, self.models[model][9 + hull], a) == SOLID
                   for model, _, a, _ in self.candidates(point, point, hull, state))

    def floor(self, x, y, navz, state='reference'):
        h = self.scene_trace((x, y, navz + 18), (x, y, navz - 128), state=state)
        if h is None:
            return 'no_floor_in_window', None
        return ('start_solid' if h['fraction'] == 0 else 'resolved'), h


def components(adj):
    # Iterative Kosaraju + undirected components; no recursion-depth dependency.
    reverse = {v: [] for v in adj}
    for v, ids in adj.items():
        for target in ids:
            if target in reverse:
                reverse[target].append(v)
    visited, order = set(), []
    for root in adj:
        stack = [(root, False)]
        while stack:
            v, leaving = stack.pop()
            if leaving:
                order.append(v)
            elif v not in visited:
                visited.add(v); stack.append((v, True))
                stack.extend((t, False) for t in adj[v] if t in adj and t not in visited)

    def groups(roots, neighbors):
        seen, result = set(), []
        for root in roots:
            if root in seen:
                continue
            group, stack = [], [root]; seen.add(root)
            while stack:
                v = stack.pop(); group.append(v)
                for target in neighbors(v):
                    if target in adj and target not in seen:
                        seen.add(target); stack.append(target)
            result.append(sorted(group))
        return result
    return groups(adj, lambda v: adj[v] + reverse[v]), groups(reversed(order), lambda v: reverse[v])


def write_csv(path, rows):
    if not rows:
        path.write_text('', encoding='utf-8')
        return
    columns = list(dict.fromkeys(key for row in rows for key in row))
    with path.open('w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=columns)
        w.writeheader(); w.writerows(rows)


def percentile(values, p):
    return round(sorted(values)[min(len(values) - 1, int(len(values) * p))], 5) if values else None


def measure_area(nav, bsp, area, density=3):
    rows = []
    x0, y0, _, x1, y1, *_ = area.g
    for ix in range(density):
        u = .1 + .8 * ix / (density - 1)
        for iy in range(density):
            v = .1 + .8 * iy / (density - 1)
            x, y = x0 + (x1 - x0) * u, y0 + (y1 - y0) * v
            z = area.z(x, y)
            status, h = bsp.floor(x, y, z)
            removed_status, removed = bsp.floor(x, y, z, 'removed_open')
            row = dict(area=area.id, attributes=area.attr, density=density, x=x, y=y,
                       nav_z=z, state='compiled_reference_intact', status=status,
                       floor_z=h['z'] if h else None,
                       model=h['model'] if h else None, policy=h['policy'] if h else None,
                       removed_open_status=removed_status,
                       removed_open_floor_z=removed['z'] if removed else None)
            if status == 'resolved':
                floor = h['z']
                row.update(error=z - floor, floor_normal_z=h['normal'][2],
                           stand_solid=int(bsp.scene_solid((x, y, floor + 36.1), 1)),
                           crouch_solid=int(bsp.scene_solid((x, y, floor + 18.1), 3)),
                           stand_at_plus18_solid=int(bsp.scene_solid((x, y, floor + 54.1), 1)))
                ceiling = bsp.scene_trace((x, y, floor + .1), (x, y, floor + 128))
                row['ceiling_within128'] = ceiling['z'] - floor if ceiling else None
                row['state_sensitive'] = int(removed_status != 'resolved' or abs(removed['z'] - floor) > 2)
            rows.append(row)
    return rows


def portal(a, b, direction):
    # Source/destination boundary witnesses, 1 unit inside each rectangle.
    x0, y0, _, x1, y1, *_ = a.g
    bx0, by0, _, bx1, by1, *_ = b.g
    if direction in (0, 2):
        lo, hi = max(x0, bx0), min(x1, bx1)
        x = (lo + hi) / 2
        sx, tx = min(x1, max(x0, x)), min(bx1, max(bx0, x))
        sy, ty = (y0 + min(1, (y1-y0)/2), by1 - min(1, (by1-by0)/2)) if direction == 0 else (
            y1 - min(1, (y1-y0)/2), by0 + min(1, (by1-by0)/2))
    else:
        lo, hi = max(y0, by0), min(y1, by1)
        y = (lo + hi) / 2
        sy, ty = min(y1, max(y0, y)), min(by1, max(by0, y))
        sx, tx = (x1 - min(1, (x1-x0)/2), bx0 + min(1, (bx1-bx0)/2)) if direction == 1 else (
            x0 + min(1, (x1-x0)/2), bx1 - min(1, (bx1-bx0)/2))
    return (sx, sy, a.z(sx, sy)), (tx, ty, b.z(tx, ty)), max(0, hi - lo)


def scene_cross_width(bsp, point):
    spans = []
    for axis in (0, 1):
        distances = []
        for sign in (-1, 1):
            end = list(point); end[axis] += sign * 128
            h = bsp.scene_trace(point, tuple(end))
            distances.append(128 * h['fraction'] if h else 128)
        spans.append(sum(distances))
    return spans


def graph_evidence(nav, bsp):
    adj = {a.id: [v for ids in a.links for v in ids if v in nav.by_id] for a in nav.areas}
    weak, strong = components(adj)
    edge_set = {(v, t) for v, ids in adj.items() for t in ids}
    filtered = {v: [t for t in ids if not (nav.by_id[v].attr & 2 and nav.by_id[t].attr & 2)] for v, ids in adj.items()}
    fw, fs = components(filtered)
    spawns, objectives = [], []
    for e in bsp.entities:
        cl = e.get('classname', '')
        if cl in ('info_player_start', 'info_player_deathmatch', 'info_vip_start') and 'origin' in e:
            point = tuple(map(float, e['origin'].split()))
            feet = point[2] - 36
            matched = sorted(nav.containing(point[0], point[1], feet, 72), key=lambda a: (abs(a.z(*point[:2]) - feet), a.id))
            spawns.append(dict(classname=cl, origin=point, feet_z=feet, area=matched[0].id if matched else None))
        if cl in ('func_bomb_target', 'info_bomb_target', 'func_hostage_rescue', 'hostage_entity', 'func_vip_safetyzone', 'func_escapezone'):
            ids = []
            if e.get('model', '').startswith('*'):
                m = bsp.models[int(e['model'][1:])]
                o = tuple(map(float, e.get('origin', '0 0 0').split()))
                for a in nav.areas:
                    x, y, z = a.center()
                    if m[0]+o[0] <= x <= m[3]+o[0] and m[1]+o[1] <= y <= m[4]+o[1] and m[2]+o[2]-18 <= z <= m[5]+o[2]+18:
                        ids.append(a.id)
            elif 'origin' in e:
                x, y, z = map(float, e['origin'].split())
                ids = [a.id for a in nav.containing(x, y, z - (36 if cl == 'hostage_entity' else 0), 72)]
            objectives.append(dict(classname=cl, model=e.get('model'), origin=e.get('origin'), areas=ids))
    reachability = []
    for s in spawns:
        if s['area'] is None:
            continue
        seen, stack = {s['area']}, [s['area']]
        while stack:
            for t in adj[stack.pop()]:
                if t not in seen:
                    seen.add(t); stack.append(t)
        reachability.append(dict(start_area=s['area'], reachable_area_count=len(seen),
                                 objective_reachable=[bool(seen.intersection(o['areas'])) if o['areas'] else None for o in objectives]))
    ladders = []
    for model, policy, origin, angles, e in bsp.brushes:
        if policy != 'ladder':
            continue
        m = bsp.models[model]; rows = {}
        for name, z in (('bottom', m[2]+origin[2]), ('top', m[5]+origin[2])):
            ids = []
            for a in nav.areas:
                x, y, az = a.center()
                dx = max(m[0]+origin[0]-a.g[3], a.g[0]-m[3]-origin[0], 0)
                dy = max(m[1]+origin[1]-a.g[4], a.g[1]-m[4]-origin[1], 0)
                if math.hypot(dx, dy) <= 64 and abs(az-z) <= 72:
                    ids.append(a.id)
            rows[name] = ids
        ladders.append(dict(model=model, bounds=m[:6], origin=origin, angles=angles, candidates=rows))
    return dict(edges=len(edge_set), connection_entries=sum(map(len,adj.values())), one_way_edges=sum((t, v) not in edge_set for v, t in edge_set),
                weak_component_sizes=sorted(map(len, weak), reverse=True),
                strong_component_sizes=sorted(map(len, strong), reverse=True),
                jump_to_jump_edges=sum(1 for v, t in edge_set if nav.by_id[v].attr & 2 and nav.by_id[t].attr & 2),
                query_filtered_weak_sizes=sorted(map(len, fw), reverse=True),
                query_filtered_strong_sizes=sorted(map(len, fs), reverse=True),
                spawns=spawns, objectives=objectives, raw_graph_reachability=reachability,
                ladders=ladders, ladder_serialization='NAV v5 contains no ladder list; entity-derived runtime links are outside raw graph')


def finding_witness(rows):
    errors = [row for row in rows if 'error' in row]
    worst = max(errors, key=lambda row: abs(row['error'])) if errors else None
    if worst and abs(worst['error']) > 2:
        return worst
    return next((row for row in rows if row['status'] != 'resolved'), None)


def audit_map(name, nav, bsp, findings, links, widths, coverage):
    all_errors, counts, area_counts = [], collections.Counter(), collections.Counter()
    attrs = collections.Counter(a.attr for a in nav.areas)
    for a in nav.areas:
        rows = measure_area(nav, bsp, a)
        errors = [r for r in rows if 'error' in r]
        for row in rows:
            counts[row['status']] += 1
            for key in ('stand_solid', 'crouch_solid', 'stand_at_plus18_solid', 'state_sensitive'):
                counts[key] += row.get(key, 0)
            if row.get('ceiling_within128') is not None:
                counts['ceiling_lt72'] += row['ceiling_within128'] < 72
                counts['ceiling_lt36'] += row['ceiling_within128'] < 36
        all_errors.extend(abs(r['error']) for r in errors)
        worst = max(errors, key=lambda r: abs(r['error'])) if errors else None
        for threshold in (2, 18):
            if worst and abs(worst['error']) > threshold:
                area_counts[f'error_gt{threshold}'] += 1
                if not a.attr & 2:
                    area_counts[f'ordinary_error_gt{threshold}'] += 1
        w = finding_witness(rows)
        if w is not None:
            findings.append(dict(map=name, **w, sampled_points=9,
                                 start_solid_points=sum(r['status'] == 'start_solid' for r in rows),
                                 unresolved_points=sum(r['status'] != 'resolved' for r in rows),
                                 category='近似・状態依存' if w['status']=='resolved' else '実機確認待ち', confidence='offline_geometry_candidate',
                                 engine_acceptance='未実施', evidence='BSP hull0; 3x3 interior samples; entity policy in summary.json'))
        x, y, nz = a.center()
        st, h = bsp.floor(x, y, nz)
        if st == 'resolved':
            span18 = scene_cross_width(bsp, (x, y, h['z'] + 18))
            span54 = scene_cross_width(bsp, (x, y, h['z'] + 54))
            widths.append(dict(map=name, area=a.id, x=x, y=y, floor_z=h['z'],
                               nav_min_extent=min(a.g[3]-a.g[0], a.g[4]-a.g[1]),
                               low_span_x=span18[0], low_span_y=span18[1], high_span_x=span54[0], high_span_y=span54[1],
                               stand_solid=int(bsp.scene_solid((x,y,h['z']+36.1),1)),
                               crouch_solid=int(bsp.scene_solid((x,y,h['z']+18.1),3)),
                               state='compiled_reference_intact', note='cardinal point cross-sections capped at 256; not route width'))
    edge_set = {(a.id, t) for a in nav.areas for ids in a.links for t in ids}
    link_counts = collections.Counter()
    for a in nav.areas:
        for direction, ids in enumerate(a.links):
            for target in ids:
                if target not in nav.by_id:
                    continue
                b = nav.by_id[target]; p, q, width = portal(a, b, direction)
                gapx = max(a.g[0]-b.g[3], b.g[0]-a.g[3], 0)
                gapy = max(a.g[1]-b.g[4], b.g[1]-a.g[4], 0)
                stp, hp = bsp.floor(*p); stq, hq = bsp.floor(*q)
                row = dict(map=name, source=a.id, target=target, direction=direction,
                           source_attributes=a.attr, target_attributes=b.attr,
                           one_way=int((target,a.id) not in edge_set), portal_overlap=width,
                           xy_gap=math.hypot(gapx,gapy), source_x=p[0], source_y=p[1], target_x=q[0], target_y=q[1],
                           nav_boundary_dz=q[2]-p[2], source_floor_status=stp, target_floor_status=stq,
                           state='compiled_reference_intact', engine_acceptance='未実施')
                if stp == stq == 'resolved':
                    row['physical_endpoint_dz'] = hq['z']-hp['z']
                    for hull, height, label in ((1,36,'stand_chord'),(3,18,'crouch_chord')):
                        start=(p[0],p[1],hp['z']+height+.1); end=(q[0],q[1],hq['z']+height+.1)
                        hit=bsp.scene_trace(start,end,hull)
                        row[label+'_blocked']=int(hit is not None)
                        row[label+'_start_solid']=int(hit is not None and hit['fraction']==0)
                        link_counts[label+'_blocked']+=hit is not None
                    row['note']='straight hull chord only; jump/step/slide/ladder not simulated'
                else:
                    row['note']='unresolved floor witness; no hull chord verdict'
                link_counts['xy_gap_positive'] += row['xy_gap'] > .01
                links.append(row)
    # Independent world-face witnesses: not a full walkable-surface coverage proof.
    world=bsp.models[0]; coverage_counts=collections.Counter()
    for fi in range(world[14],world[14]+world[15]):
        f=bsp.faces[fi]; plane=bsp.planes[f[0]]; normal=tuple(v*(-1 if f[1] else 1) for v in plane[:3])
        if normal[2]<.7 or f[3]<3:
            continue
        vertices=[]
        for se in bsp.surfedges[f[2]:f[2]+f[3]]:
            edge=bsp.edges[abs(se)]; vertices.append(bsp.vertices[edge[0] if se>=0 else edge[1]])
        x,y,z=tuple(sum(v[i] for v in vertices)/len(vertices) for i in range(3))
        if not bsp.scene_solid((x,y,z-.2),0) or bsp.scene_solid((x,y,z+.2),0):
            continue
        stand=not bsp.scene_solid((x,y,z+36.2),1); crouch=not bsp.scene_solid((x,y,z+18.2),3)
        if not stand and not crouch:
            continue
        matched18=nav.containing(x,y,z,18); matched64=nav.containing(x,y,z,64)
        coverage_counts['eligible_face_centroids']+=1
        coverage_counts['without_nav_within18']+=not matched18
        coverage_counts['without_nav_within64']+=not matched64
        if not matched18:
            coverage.append(dict(map=name,face=fi,x=x,y=y,z=z,stand_fit=int(stand),crouch_fit=int(crouch),
                                 nav_within64=';'.join(str(a.id) for a in matched64),
                                 category='実機確認待ち', note='isolated roofs/props also qualify; reachability not established'))
    overlaps=collections.Counter()
    for i,a in enumerate(nav.areas):
        for b in nav.areas[i+1:]:
            x0,y0,x1,y1=max(a.g[0],b.g[0]),max(a.g[1],b.g[1]),min(a.g[3],b.g[3]),min(a.g[4],b.g[4])
            if x1>x0 and y1>y0:
                dz=abs(a.z((x0+x1)/2,(y0+y1)/2)-b.z((x0+x1)/2,(y0+y1)/2))
                overlaps['overlap_xy_pairs']+=1; overlaps['separate_floor_pairs_gt72']+=dz>72
    return dict(map=name,areas=len(nav.areas),places=len(nav.places),attributes=dict(attrs),
                reference_errors=nav.errors,brush_policies=dict(bsp.policies),
                unresolved_brush_entities=[dict(model=i,classname=e.get('classname'),spawnflags=e.get('spawnflags'),origin=o,angles=an)
                    for i,policy,o,an,e in bsp.brushes if policy=='unresolved_entity_policy'],
                samples=dict(counts),area_candidates=dict(area_counts),
                error_median=percentile(all_errors,.5),error_p95=percentile(all_errors,.95),
                error_max=round(max(all_errors),5) if all_errors else None,
                links=dict(link_counts),coverage=dict(coverage_counts),overlaps=dict(overlaps),
                graph=graph_evidence(nav,bsp),
                production_expectation=dict(version=5,bsp_size=nav.bsp_size,places=nav.places,
                    area_digests={str(a.id):a.digest() for a in nav.areas}))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    manifest,summary,findings,links,widths,coverage,priority,sensitivity=[],[],[],[],[],[],[],[]
    for path in sorted(args.maps.glob('*.nav')):
        bp=path.with_suffix('.bsp')
        if not bp.is_file():
            continue
        raw,br=path.read_bytes(),bp.read_bytes(); nav,bsp=Nav(raw),Bsp(br)
        manifest.append(dict(map=path.stem,nav_path=str(path.resolve()),bsp_path=str(bp.resolve()),
                             nav_bytes=len(raw),bsp_bytes=len(br),nav_sha256=sha(raw),bsp_sha256=sha(br),
                             nav_version=nav.version,bsp_version=30,header_bsp_bytes=nav.bsp_size,
                             size_matches=nav.bsp_size==len(br),generator='unverified',generation_settings='unknown',
                             source_pair_provenance='same map basename/directory and size only; SHA identifies audited files, not historical generation input'))
        print('auditing',path.stem,flush=True)
        summary.append(audit_map(path.stem,nav,bsp,findings,links,widths,coverage))
        if path.stem=='de_dust2':
            for identity in PRIORITY:
                a=nav.by_id[identity]
                for density in (3,7,15):
                    rows=measure_area(nav,bsp,a,density)
                    for row in rows:
                        row['map']=path.stem
                    priority.extend(rows)
                    errors=[abs(r['error']) for r in rows if 'error' in r]
                    sensitivity.append(dict(area=identity,density=density,samples=len(rows),
                                            resolved=len(errors),max_error=max(errors) if errors else None,
                                            error_gt18=sum(v>18 for v in errors),
                                            start_solid=sum(r['status']=='start_solid' for r in rows),
                                            state_sensitive=sum(r.get('state_sensitive',0) for r in rows)))
    if len(manifest)!=16:
        raise ValueError(f'expected frozen cohort of 16 pairs, got {len(manifest)}')
    (args.out/'input_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (args.out/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for name,rows in (('findings',findings),('links',links),('widths',widths),('coverage_candidates',coverage),
                      ('dust2_priority_samples',priority),('density_sensitivity',sensitivity)):
        write_csv(args.out/(name+'.csv'),rows)
    print(json.dumps(dict(maps=len(summary),areas=sum(v['areas'] for v in summary),findings=len(findings),
                          links=len(links),coverage_candidates=len(coverage),priority_samples=len(priority))))


if __name__=='__main__':
    main()
