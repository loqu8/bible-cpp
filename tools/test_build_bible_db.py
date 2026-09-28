#!/usr/bin/env python3
"""Unit tests for tools/build_bible_db.py — Strong's tag handling.

Run with:
    cd ~/repos/loqu8/bible-cpp
    python3 -m unittest tools.test_build_bible_db -v

The corpus test reads data/raw/kjv_strongs.zip and is skipped when it has
not been downloaded yet (run build_bible_db.py once).
"""

import os
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tools.build_bible_db import (
    extract_verses_txt,
    normalise_paren_spacing,
    parse_bss_module,
    repair_tvm_tags,
    strip_morphology,
    strip_strongs,
)

KJV_ZIP = Path(__file__).resolve().parent.parent / "data" / "raw" / "kjv_strongs.zip"

# Bible SuperSearch KJV, Psalm 35:3 as shipped (2021-09-28): the TVM codes
# on "stop", "against", "persecute" and "say" lost their opening parenthesis.
PS_35_3 = (
    "Draw out{H7324}{(H8685)} also the spear{H2595}, and stop{H5462}{H8798)} "
    "the way against{H7125}{H8800)} them that persecute{H7291}{H8802)} me: "
    "say{H559}{H8798)} unto my soul{H5315}, I am thy salvation{H3444}."
)

BROKEN_TAG = re.compile(r'\{[HG]\d+\)\}')


class RepairTvmTagsTest(unittest.TestCase):
    def test_should_restore_opening_parenthesis_when_tvm_tag_is_broken(self):
        self.assertEqual(repair_tvm_tags("stop{H5462}{H8798)} the"),
                         "stop{H5462}{(H8798)} the")

    def test_should_leave_well_formed_tags_unchanged(self):
        text = "Draw out{H7324}{(H8685)} also the spear{H2595}{G2316}{(G5627)}"
        self.assertEqual(repair_tvm_tags(text), text)

    def test_should_repair_every_broken_tag_in_a_verse(self):
        repaired = repair_tvm_tags(PS_35_3)
        self.assertIsNone(BROKEN_TAG.search(repaired))
        self.assertEqual(repaired.count("{(H"), 5)

    def test_should_produce_clean_text_when_parsing_a_damaged_verse(self):
        line = f"19|35|3|{PS_35_3}"
        [(_, _, _, text, text_plain)] = parse_bss_module(line)
        self.assertEqual(
            text,
            "Draw out{H7324} also the spear{H2595}, and stop{H5462} the way "
            "against{H7125} them that persecute{H7291} me: say{H559} unto my "
            "soul{H5315}, I am thy salvation{H3444}.")
        self.assertEqual(
            text_plain,
            "Draw out also the spear, and stop the way against them that "
            "persecute me: say unto my soul, I am thy salvation.")


class NormaliseParenSpacingTest(unittest.TestCase):
    def test_should_add_space_before_paren_when_glued_to_punctuation(self):
        self.assertEqual(normalise_paren_spacing("and Casluhim{H3695},(out of whom"),
                         "and Casluhim{H3695}, (out of whom")

    def test_should_add_space_before_paren_when_glued_to_a_tag(self):
        self.assertEqual(normalise_paren_spacing("of Bela{H1106}(the same"),
                         "of Bela{H1106} (the same")

    def test_should_remove_space_after_opening_paren(self):
        self.assertEqual(normalise_paren_spacing("thither,( is it not"),
                         "thither, (is it not")

    def test_should_not_add_space_when_paren_opens_the_verse(self):
        self.assertEqual(normalise_paren_spacing("( For the men of war"),
                         "(For the men of war")

    def test_should_leave_tvm_tags_unchanged(self):
        text = "came{H3318}{(H8804)} Philistim{H6430},)"
        self.assertEqual(normalise_paren_spacing(text), text)

    def test_should_produce_spaced_plain_text_when_parsing(self):
        line = ("1|10|14|And Pathrusim{H6625}, and Casluhim{H3695},(out of whom "
                "came{H3318}{(H8804)} Philistim{H6430},) and Caphtorim{H3732}.")
        [(_, _, _, text, text_plain)] = parse_bss_module(line, space_parens=True)
        self.assertEqual(text_plain,
                         "And Pathrusim, and Casluhim, (out of whom came "
                         "Philistim,) and Caphtorim.")
        self.assertIn("Casluhim{H3695}, (out", text)

    def test_should_leave_cuv_parens_alone_when_not_asked(self):
        line = "1|4|1|生了该隐{H7014}(就是得的意思)，便说"
        [(_, _, _, text, _)] = parse_bss_module(line)
        self.assertEqual(text, "生了该隐{H7014}(就是得的意思)，便说")


@unittest.skipUnless(KJV_ZIP.exists(), "data/raw/kjv_strongs.zip not downloaded")
class KjvCorpusTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = extract_verses_txt(KJV_ZIP)

    def test_should_find_the_known_damage_in_the_raw_source(self):
        broken = BROKEN_TAG.findall(self.raw)
        self.assertEqual(len(broken), 2485)
        # All TVM codes (H8675-H8804), never lemma numbers.
        numbers = [int(t[2:-2]) for t in broken]
        self.assertGreaterEqual(min(numbers), 8675)
        self.assertLessEqual(max(numbers), 8804)

    def test_should_leave_no_broken_tags_after_repair(self):
        repaired = repair_tvm_tags(self.raw)
        self.assertEqual(len(BROKEN_TAG.findall(repaired)), 0)
        self.assertEqual(len(re.findall(r'\{\([HG]\d+\)\}', repaired)),
                         68873 + 28915 + 2485)

    def test_should_keep_verse_count_and_clean_every_verse(self):
        verses = parse_bss_module(self.raw, space_parens=True)
        self.assertEqual(len(verses), 31102)
        for book, chapter, verse, text, text_plain in verses:
            where = f"{book}:{chapter}:{verse}"
            self.assertNotIn("{", text_plain, where)
            self.assertNotIn("}", text_plain, where)
            # Tagged text keeps only lexical tags {H#}/{G#}.
            leftover = re.sub(r'\{[HG]\d+\}', '', text)
            self.assertNotIn("{", leftover, where)
            self.assertNotIn("}", leftover, where)
            self.assertIsNone(re.search(r'\S\(|\( ', text_plain), where)


if __name__ == "__main__":
    unittest.main()
