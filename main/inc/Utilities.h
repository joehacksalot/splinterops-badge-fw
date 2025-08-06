/**
 * @file Utilities.h
 * @brief Common utility functions and badge type definitions
 * 
 * This module provides essential utility functions and definitions including:
 * - Badge type identification and parsing (TRON, Reactor, Crest, FMAN25)
 * - Random number generation utilities
 * - BLE device name generation
 * - Common mathematical macros (MIN, MAX)
 * - Badge variant-specific configuration support
 * 
 * These utilities are used throughout the badge system to provide
 * consistent behavior across different badge hardware variants.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

typedef enum BadgeType_e
{
    BADGE_TYPE_UNKNOWN = 0,
    BADGE_TYPE_TRON = 1,
    BADGE_TYPE_REACTOR = 2,
    BADGE_TYPE_CREST = 3,
    BADGE_TYPE_FMAN25 = 4
} BadgeType;

/**
 * @brief Get the current badge type
 * 
 * Returns the badge type for this device, determined from hardware
 * configuration or stored settings. Used for badge-specific behavior
 * and feature enablement.
 * 
 * @return BadgeType enum value for this badge
 */
extern BadgeType GetBadgeType(void);

/**
 * @brief Parse badge type from numeric value
 * 
 * Converts a numeric badge type identifier to the corresponding
 * BadgeType enum value. Used for configuration parsing and validation.
 * 
 * @param badgeTypeNum Numeric badge type identifier
 * @return BadgeType enum value, or BADGE_TYPE_UNKNOWN if invalid
 */
extern BadgeType ParseBadgeType(int badgeTypeNum);
/**
 * @brief Generate a random number within specified range
 * 
 * Generates a cryptographically secure random number within the specified
 * minimum and maximum bounds using ESP-IDF's hardware random number generator.
 * 
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return Random number between min and max (inclusive)
 */
extern uint32_t GetRandomNumber(uint32_t min, uint32_t max);

/**
 * @brief Get the badge's BLE device name
 * 
 * Retrieves the unique BLE device name for this badge, typically based on
 * the badge type and unique identifier. Used for BLE advertising and
 * badge-to-badge identification.
 * 
 * @param buffer Output buffer to store the device name
 * @param bufferSize Size of the output buffer in bytes
 */
extern void GetBadgeBleDeviceName(char * buffer, uint32_t bufferSize);

#endif // UTILITIES_H_