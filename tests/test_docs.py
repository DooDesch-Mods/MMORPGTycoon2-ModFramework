from pathlib import Path
import copy,json,sys,unittest
import jsonschema
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import generate_docs
import build_docs
from mt2fw import packages
class DocumentationContractTests(unittest.TestCase):
    def test_source_machine_links_resolve_in_repository(self):
        import re
        from urllib.parse import urlsplit
        for name in ('agents/start.md','reference/manifests.md'):
            page=ROOT/'docs'/name
            for href in re.findall(r'\]\(([^)]+)\)',page.read_text(encoding='utf-8')):
                url=urlsplit(href)
                if url.scheme or url.netloc or not url.path:continue
                with self.subTest(page=name,href=href):
                    self.assertFalse(url.path.startswith('/'))
                    self.assertTrue((page.parent/url.path).is_file())

    def test_exported_machine_links_work_in_nested_and_versioned_markdown(self):
        text='[API](../public/api/1.0.0/api.json) [Index](../public/llms.txt)'
        self.assertEqual(build_docs.markdown_assets(text,Path('agents/start.md')),
                         '[API](../api/1.0.0/api.json) [Index](../llms.txt)')
        self.assertEqual(build_docs.markdown_assets(text,Path('versions/1.0.0/agents/start.md')),
                         '[API](../../../api/1.0.0/api.json) [Index](../../../llms.txt)')

    def test_contract_requires_effects_and_known_capabilities(self):
        contract=generate_docs.read(ROOT/'api/contract.json')
        for field in ('returns','thread','lifecycle','side_effects','errors','example'):
            invalid=copy.deepcopy(contract);del invalid['functions'][0][field]
            with self.subTest(field=field),self.assertRaises(ValueError):generate_docs.validate(invalid)
        invalid=copy.deepcopy(contract);invalid['functions'][0]['capabilities']=['unknown']
        with self.assertRaises(ValueError):generate_docs.validate(invalid)

    def test_manifest_schema_matches_semantic_validator_for_documented_cases(self):
        contract=generate_docs.read(ROOT/'api/contract.json');schema=generate_docs.schema(contract)
        checker=jsonschema.FormatChecker()
        @checker.checks('semver',raises=(ValueError,TypeError))
        def semver(value):packages.version(value);return True
        @checker.checks('supported-mt2-api',raises=(ValueError,TypeError))
        def supported(value):return packages.version(value)[0]==1 and packages.version(value)<=packages.version('1.0.0')
        @checker.checks('mt2-relative-path',raises=(ValueError,TypeError))
        def relative(value):packages.relative(value);return True
        validator=jsonschema.Draft202012Validator(schema,format_checker=checker)
        valid=packages.read_json(ROOT/'templates/hello/mod.json')
        cases=[valid,{**valid,'version':'1.2.0-beta.1+build'},
               {**valid,'entry':'../escape.lua'},{**valid,'api_version':'2.0.0'},
               {**valid,'version':'1.0.0-01'},{**valid,'capabilities':['objects.write']},
               {key:value for key,value in valid.items() if key!='capabilities'}]
        for item in cases:
            expected=True
            try:packages.validate_manifest(item)
            except (ValueError,TypeError):expected=False
            with self.subTest(manifest=item):self.assertEqual(validator.is_valid(item),expected)

    def test_generated_outputs_match_versioned_sources_and_examples(self):
        contract=generate_docs.read(ROOT/'api/contract.json')
        for name,body in generate_docs.outputs(contract).items():
            with self.subTest(output=name):self.assertEqual((ROOT/name).read_text(encoding='utf-8'),body)
    def test_screenshot_inventory_has_actual_files_and_rights(self):
        media=generate_docs.read(ROOT/'docs/media.json')
        for image in media['images']:
            with self.subTest(image=image['id']):
                self.assertTrue((ROOT/'docs'/image['path']).is_file())
                self.assertTrue(all(image.get(key) for key in ('rights','origin','locale','caption','alt','sha256')))
if __name__=='__main__':unittest.main()
