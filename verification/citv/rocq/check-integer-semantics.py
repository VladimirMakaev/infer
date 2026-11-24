#!/usr/bin/env python3
"""Compare fresh OCaml/Zarith bindings with their configured Rocq translation.

Only nonzero divisors are judged. Every run uses its own cleaned temporary tree.
Two configuration mutants must produce compilable bindings but false equations.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    home = Path(os.environ.get('CITV_HOME', str(Path.home()/'.citv-rocq')))
    opam = str(home/'bin/opam')
    root = os.environ.get('CITV_OPAMROOT', str(home/'opamroot'))
    switch = os.environ.get('CITV_SWITCH', 'coq541')
    prefix = [opam, 'exec', '--root='+root, '--switch='+switch, '--']
    translator = os.environ.get('CITV_TRANSLATOR', str(home/'bin/rocqOfOCaml.exe'))
    with tempfile.TemporaryDirectory(prefix='citv-integer-semantics-') as temporary:
        work = Path(temporary)
        def run(argv, label):
            result = subprocess.run(argv, cwd=work, text=True, capture_output=True)
            (work/(label+'.log')).write_text(result.stdout+result.stderr)
            return result
        def need(argv, label):
            result = run(argv, label)
            if result.returncode:
                raise RuntimeError(label+': '+(result.stdout+result.stderr)[-2500:])
            return result.stdout
        for name in ['DivisionBindings.ml','DivisionReference.ml']:
            shutil.copyfile(HERE/'tests'/name,work/name)
        need(prefix+['ocamlfind','ocamlc','-bin-annot','-package','zarith','-c','DivisionBindings.ml'],'ocaml-bindings')
        need(prefix+['ocamlfind','ocamlc','-package','zarith','-linkpkg','DivisionBindings.cmo',
                     'DivisionReference.ml','-o','reference'],'ocaml-reference-build')
        lines=need(prefix+[str(work/'reference')],'native-values').splitlines()
        rows=[tuple(map(int,line.split())) for line in lines]
        if len(rows)!=190 or {(a,b) for a,b,q,r in rows}!={(a,b) for a in range(-9,10) for b in range(-5,6) if b}:
            raise RuntimeError('native input grid is incomplete or duplicated')
        zarith=Path(need(prefix+['ocamlfind','query','zarith'],'zarith-path').strip())
        shutil.copyfile(zarith/'z.cmi',work/'z.cmi')
        def check(config, label):
            need(prefix+[translator,'-config',str(config),'-output','DivisionBindings.v','DivisionBindings.ml'],label+'-translate')
            text=(work/'DivisionBindings.v').read_text()
            if not text or 'op_startypeminuserrorstar' in text:
                raise RuntimeError('generated binding output is empty or contains an error marker')
            need(prefix+['rocq','compile','-Q','.','','DivisionBindings.v'],label+'-bindings')
            queries=['From Stdlib Require Import ZArith.','Require Import DivisionBindings.','Open Scope Z_scope.']
            for index,(a,b,q,r) in enumerate(rows):
                for method,value in [('int_div',q),('int_rem',r),('zarith_div',q),('zarith_rem',r)]:
                    name=f'case_{index}_{method}'
                    queries += [f'Example {name} : DivisionBindings.{method} ({a}) ({b}) = ({value}).',
                                'Proof. reflexivity. Qed.',f'Print Assumptions {name}.']
            (work/'IntegerCases.v').write_text('\n'.join(queries)+'\n')
            result=run(prefix+['rocq','compile','-Q','.','','IntegerCases.v'],label+'-cases')
            return result
        control=check(HERE/'integer-semantics.json','control')
        count=sum(line.strip()=='Closed under the global context' for line in control.stdout.splitlines())
        if control.returncode or count!=760 or 'Axioms:' in control.stdout:
            raise RuntimeError('configured integer semantics failed: '+(control.stdout+control.stderr)[-2500:])
        print('integer semantics:190/190 nonzero pairs,760/760 fresh closed equations,0 output markers')
        if args.self_test:
            base=json.loads((HERE/'integer-semantics.json').read_text())
            for label,changes,method in [
                ('floor-quotient',{'Stdlib.op_div':'Z.div','Z.div':'Z.div'},'int_div'),
                ('floor-remainder',{'Stdlib._mod':'Z.modulo','Z.rem':'Z.modulo'},'int_rem')]:
                config=json.loads(json.dumps(base))
                for pair in config['renaming_rules']:
                    if pair[0] in changes:pair[1]=changes[pair[0]]
                path=work/(label+'.json');path.write_text(json.dumps(config))
                result=check(path,label)
                if result.returncode==0:
                    raise RuntimeError(label+' survived the semantic comparison')
                output=result.stdout+result.stderr
                line=re.search(r'line (\d+)',output)
                if not line:raise RuntimeError(label+' did not identify its failing equation')
                source=(work/'IntegerCases.v').read_text().splitlines()
                prior=[text for text in source[:int(line.group(1))] if text.startswith('Example ')]
                if not prior or ('_'+method+' :') not in prior[-1]:
                    raise RuntimeError(label+' failed outside its intended equation')
                print(label+': refused at '+prior[-1].split()[1]+'; bindings compiled')


if __name__=='__main__':
    try:main()
    except (OSError, RuntimeError, ValueError) as failure:
        print('integer semantics: FAILED:',failure)
        raise SystemExit(1)
