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
    KJV_CORRECTIONS,
    extract_verses_txt,
    kjv_notes,
    normalise_paren_spacing,
    parse_bss_module,
    parse_usfm_headings,
    repair_tvm_tags,
    split_heading,
    split_subscription,
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


class KjvCorrectionsTest(unittest.TestCase):
    def test_should_apply_correction_when_source_matches(self):
        line = "1|25|4|and Hanoch{H2585}, and Abida{H28}, and Eldaah{H420}."
        [(_, _, _, text, text_plain)] = parse_bss_module(line, corrections=KJV_CORRECTIONS)
        self.assertEqual(text, "and Hanoch{H2585}, and Abidah{H28}, and Eldaah{H420}.")
        self.assertEqual(text_plain, "and Hanoch, and Abidah, and Eldaah.")

    def test_should_render_italic_but_as_plain_text_in_1_john_2_23(self):
        line = ("62|2|23|not{G3761} the Father{G3962}:(but) he that "
                "acknowledgeth{G3670}{(G5723)} the Son{G5207}")
        [(_, _, _, _, text_plain)] = parse_bss_module(
            line, space_parens=True, corrections=KJV_CORRECTIONS)
        self.assertEqual(text_plain, "not the Father: but he that acknowledgeth the Son")

    def test_should_leave_other_verses_alone(self):
        line = "13|1|33|and Henoch{H2585}, and Abida{H28}, and Eldaah{H420}."
        [(_, _, _, text, _)] = parse_bss_module(line, corrections=KJV_CORRECTIONS)
        self.assertIn("Abida{H28},", text)

    def test_should_fail_when_correction_no_longer_matches(self):
        line = "1|25|4|and Hanoch{H2585}, and Abidah{H28}, and Eldaah{H420}."
        with self.assertRaises(ValueError):
            parse_bss_module(line, corrections=KJV_CORRECTIONS)


class KjvNotesTest(unittest.TestCase):
    def test_should_split_psalm_title_from_verse(self):
        self.assertEqual(
            split_heading("A Psalm of David. The LORD is my shepherd", "A Psalm of David."),
            "A Psalm of David.")

    def test_should_keep_our_spelling_when_edition_hyphenates(self):
        ours = "To the chief Musician, when he had gone in to Bathsheba. Have mercy upon me"
        self.assertEqual(split_heading(ours, "To the chief Musician, when he had gone in to Bath-sheba."),
                         "To the chief Musician, when he had gone in to Bathsheba.")

    def test_should_split_title_ending_mid_sentence(self):
        ours = "from the hand of Saul: And he said, I will love thee, O LORD"
        self.assertEqual(split_heading(ours, "from the hand of Saul: And he said,"),
                         "from the hand of Saul: And he said,")

    def test_should_fail_when_heading_does_not_prefix_verse(self):
        with self.assertRaises(ValueError):
            split_heading("Blessed are the undefiled", "ALEPH.")

    def test_should_take_text_after_final_amen_as_subscription(self):
        self.assertEqual(
            split_subscription("Grace be with you. Amen. To the Galatians written from Rome."),
            "To the Galatians written from Rome.")

    def test_should_find_no_subscription_when_verse_ends_with_amen(self):
        self.assertIsNone(split_subscription("unto the end of the world. Amen."))

    def test_should_collect_notes_for_headed_and_closing_verses(self):
        verses = [
            (19, 23, 1, "", "A Psalm of David. The LORD is my shepherd"),
            (19, 23, 2, "", "He maketh me to lie down"),
            (48, 6, 18, "", "be with your spirit. Amen. To the Galatians written from Rome."),
        ]
        notes = kjv_notes(verses, {(19, 23, 1): "A Psalm of David."}, expect=(1, 1))
        self.assertEqual(notes, {(19, 23, 1): ("A Psalm of David.", None),
                                 (48, 6, 18): (None, "To the Galatians written from Rome.")})


USFM_ZIP = KJV_ZIP.parent / "eng-kjv_usfm.zip"


@unittest.skipUnless(KJV_ZIP.exists() and USFM_ZIP.exists(), "raw KJV sources not downloaded")
class KjvNotesCorpusTest(unittest.TestCase):
    def test_should_find_every_heading_and_subscription(self):
        verses = parse_bss_module(extract_verses_txt(KJV_ZIP), space_parens=True,
                                  corrections=KJV_CORRECTIONS)
        headings = parse_usfm_headings(USFM_ZIP)
        self.assertEqual(len(headings), 116 + 22)
        notes = kjv_notes(verses, headings)
        plain = {(b, c, v): p for b, c, v, _, p in verses}
        self.assertEqual(sum(1 for h, _ in notes.values() if h), 138)
        self.assertEqual(sorted(k[0] for k, (_, s) in notes.items() if s), list(range(45, 59)))
        for key, (heading, subscription) in notes.items():
            if heading:
                self.assertTrue(plain[key].startswith(heading + " "), key)
            if subscription:
                self.assertTrue(plain[key].endswith(" " + subscription), key)
        self.assertEqual(notes[(19, 119, 1)], ("ALEPH.", None))


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
        verses = parse_bss_module(self.raw, space_parens=True,
                                  corrections=KJV_CORRECTIONS)
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
