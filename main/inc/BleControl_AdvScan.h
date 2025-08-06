/**
 * @file BleControl_AdvScan.h
 * @brief BLE advertising and scanning functionality
 * 
 * This module provides BLE advertisement and scanning capabilities including:
 * - BLE advertisement broadcasting for badge discovery
 * - Peer badge scanning and discovery
 * - Advertisement payload management
 * - Scan result processing and filtering
 * - Integration with main BLE control system
 * 
 * The advertising and scanning system enables badges to discover each other
 * for interactive gaming and communication features.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef BLECONTROL_ADV_SCAN_H
#define BLECONTROL_ADV_SCAN_H

#include "BleControl.h"

/**
 * @brief Start BLE advertisement scanning for peer badge discovery
 * 
 * Initiates BLE scanning to discover other badges in the vicinity.
 * This function configures the BLE scanner with appropriate parameters
 * and begins the scanning process for badge-to-badge communication.
 * The scan results will be processed and filtered for compatible badges.
 * 
 * @param this Pointer to BleControl instance
 */
void BleControl_StartAdvertisementScan(BleControl * this);

#endif // BLECONTROL_ADV_SCAN_H
