# game_types component (header-only)
idf_component_register(
    INCLUDE_DIRS "."
    REQUIRES
        hashmap           # Peer map type
        utilities         # Utilities.h
        badge_hw_profile  # Badge.h, BadgeHwProfile.h (BadgeType)
)
