"""Guard NDS-only battle rendering choices that host C tests cannot execute."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
ANIMATION = ROOT / "source" / "graphics" / "battle_animation.c"


class BattleRenderContractTest(unittest.TestCase):
    def test_hit_flash_never_draws_the_old_opaque_backdrop(self) -> None:
        source = ANIMATION.read_text(encoding="utf-8")
        self.assertNotIn(
            "graphicsTopFillRect(base_x + offset + 8, base_y + 8, 112, 112",
            source,
        )
        self.assertIn("battleAnimationFighterVisible", source)

    def test_enemy_orientation_uses_the_reviewed_side_policy(self) -> None:
        source = ANIMATION.read_text(encoding="utf-8")
        self.assertIn("battleAnimationHorizontalFlip(side)", source)


if __name__ == "__main__":
    unittest.main()
