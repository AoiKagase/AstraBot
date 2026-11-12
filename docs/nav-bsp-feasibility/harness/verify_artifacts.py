"""Match independently decoded semantics against current production C++ code."""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_csv(path):
    with path.open(encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--probe', type=Path)
    p.add_argument('--reference-commit', help='Commit independently obtained with git -C ../ReGameDLL_CS rev-parse HEAD')
    args = p.parse_args()
    out = args.out.resolve()
    root = Path(__file__).resolve().parents[3]
    reference_identity=out/'reference_identity.json'
    if reference_identity.exists():
        refs=json.loads(reference_identity.read_text(encoding='utf-8'))
        for relative,expected in refs['sha256'].items():
            assert digest(root/relative)==expected, f'audited reference source changed: {relative}'
    manifest = json.loads((out/'input_manifest.json').read_text(encoding='utf-8'))
    summary = json.loads((out/'summary.json').read_text(encoding='utf-8'))
    assert len(manifest) == len(summary) == 16
    for m in manifest:
        assert digest(Path(m['nav_path'])) == m['nav_sha256'], m['map']
        assert digest(Path(m['bsp_path'])) == m['bsp_sha256'], m['map']
        assert m['size_matches'], m['map']
    tests = subprocess.run([sys.executable, '-B', '-m', 'unittest', 'discover', '-s', str(out/'harness'),
                            '-p', 'test_diagnose.py'], text=True, capture_output=True)
    (out/'harness_tests.log').write_text(tests.stdout+tests.stderr, encoding='utf-8')
    assert tests.returncode == 0, tests.stderr
    paths = [root/'src/core/nav'/f'{name}.cpp' for name in ('legacy_nav_reader','nav_model','nav_snapshot','nav_query')]
    paths += [root/'include/astrabot/nav'/f'{name}.hpp' for name in ('legacy_nav_reader','nav_model','nav_snapshot','nav_query')]
    paths += [root/'src/core/nav/locomotion.cpp', root/'src/adapter/metamod/plugin_runtime.cpp',
              root/'src/adapter/metamod/observation_adapter.cpp']
    hashes = {str(path.relative_to(root)).replace('\\','/'): digest(path) for path in paths}
    if args.probe:
        assert args.probe.is_file()
        for path in paths[:8]:
            assert path.stat().st_mtime <= args.probe.stat().st_mtime, f'rebuild probe for {path}'
        points = out/'priority_points.txt'
        dense = [r for r in read_csv(out/'dust2_priority_samples.csv') if r['density']=='15' and r['status']=='resolved']
        points.write_text(''.join(f"{r['area']} {r['x']} {r['y']} {r['floor_z']} {r['nav_z']}\n" for r in dense), encoding='utf-8')
        outputs = {}
        for m, s in zip(manifest, summary):
            assert m['map'] == s['map']
            command = [str(args.probe.resolve()), m['nav_path']]
            if m['map']=='de_dust2':
                command.append(str(points))
            result = subprocess.run(command, text=True, capture_output=True, check=True)
            actual = json.loads(result.stdout)
            assert actual['read_result']==0, m['map']
            for key in ('version','bsp_size','places','area_digests'):
                assert actual[key] == s['production_expectation'][key], (m['map'],key)
            outputs[m['map']] = actual
        (out/'production_reader.json').write_text(json.dumps(outputs,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        (out/'source_identity.json').write_text(json.dumps(dict(source_sha256=hashes,
               probe_path=str(args.probe.resolve()), probe_sha256=digest(args.probe),
               production_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
               reference_head=args.reference_commit,
               reference_identity_method='supplied from a separate read-only git command; child-process external git access is restricted on audit host',
               source_state='dirty working tree; hashes, not HEAD alone, identify the measured sources'),indent=2)+'\n',encoding='utf-8')
    else:
        recorded = json.loads((out/'source_identity.json').read_text(encoding='utf-8'))
        assert hashes == recorded['source_sha256'], 'audited source changed; rebuild and compare again'
        outputs = json.loads((out/'production_reader.json').read_text(encoding='utf-8'))
    for s in summary:
        actual=outputs[s['map']]
        assert actual['read_result']==0
        for key in ('version','bsp_size','places','area_digests'):
            assert actual[key]==s['production_expectation'][key], (s['map'],key)
    assert all(digest(path)==hashes[str(path.relative_to(root)).replace('\\','/')] for path in paths)
    queries = outputs['de_dust2']['point_queries']
    delta = max(abs(r['production_nav_z']-r['predicted_nav_z']) for r in queries)
    assert delta <= .001, delta  # float32 production vs Python double evaluation
    query_stats = []
    for area in sorted(set(q['source_area'] for q in queries)):
        rows = [q for q in queries if q['source_area']==area]
        query_stats.append(dict(area=area,points=len(rows),no_area_tolerance16=sum(q['result16']==6 for q in rows),
                                no_area_tolerance64=sum(q['result64']==6 for q in rows),
                                different_area_tolerance64=sum(q['match64']!=area for q in rows)))
    counts={name:len(read_csv(out/(name+'.csv'))) for name in
            ('findings','links','widths','coverage_candidates','dust2_priority_samples','density_sensitivity')}
    for row in read_csv(out/'findings.csv'):
        if row['status']=='resolved':
            assert abs(float(row['error']))>2, ('nonreproducing candidate witness',row['map'],row['area'])
        else:
            assert row['category']=='実機確認待ち', (row['map'],row['area'])
    assert counts['links']==sum(s['graph']['connection_entries'] for s in summary)
    assert counts['dust2_priority_samples']==6*(9+49+225)
    assert counts['density_sensitivity']==18
    support=read_csv(out/'priority_hull_support.csv')
    assert len(support)==1350
    support_summary=json.loads((out/'priority_hull_summary.json').read_text(encoding='utf-8'))
    assert len(support_summary)==6
    for s in support_summary:
        rows=[r for r in support if int(r['area'])==s['area']]
        assert len(rows)==225
        for posture in ('point','standing','crouching'):
            resolved=[r for r in rows if r.get(posture+'_support_error')]
            assert s[posture]['resolved']==len(resolved)
            assert abs(s[posture]['max_error']-max(abs(float(r[posture+'_support_error'])) for r in resolved))<1e-9
    report=out/'REPORT_JA.md'
    if report.exists():
        body=report.read_text(encoding='utf-8')
        assert '<!-- MAP_TABLE -->' not in body
        for target in re.findall(r'\]\(([^)]+)\)',body):
            if not target.startswith(('http://','https://')):
                assert (out/target.split('#')[0]).is_file(), target
    evidence=dict(input_pairs=16,input_identities_rechecked=True,production_maps_matched=len(outputs),
                  production_area_semantic_digests_matched=sum(s['areas'] for s in summary),
                  production_digest_fields='IDs, attributes, 8 corner floats, place, all directional connections, hiding, approaches, encounters and spot IDs/quantized t',
                  max_floor_formula_float_error=delta,comparison_tolerance=.001,
                  priority_point_queries=query_stats,artifact_rows=counts,
                  priority_hull_support_rows=len(support),
                  harness_tests=re.search(r'Ran (\d+) tests?',tests.stderr).group(1)+' passed; see harness_tests.log',
                  live_engine_tests='not performed',generation_or_optimized_nav='not produced')
    (out/'verification.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(evidence,ensure_ascii=False))


if __name__=='__main__':
    main()
