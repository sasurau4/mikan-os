/**
 * @file pci.hpp
 *
 * @brief PCI bus driver implementation.
 */

#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci
{
    /** @brief port address for CONFIG_ADDRESS register */
    const uint16_t kConfigAddress = 0x0cf8;
    /** @brief port address for CONFIG_DATA register */
    const uint16_t kConfigData = 0x0cfc;

    /** @brief Class code for PCI device */
    struct ClassCode
    {
        uint8_t base, sub, interface;

        /** @brief return true if the base class matches */
        bool Match(uint8_t b) { return b == base; }
        /** @brief return true if the base and sub class match */
        bool Match(uint8_t b, uint8_t s) { return Match(b) && s == sub; }
        /** @brief return true if the base class, sub class, interface match */
        bool Match(uint8_t b, uint8_t s, uint8_t i)
        {
            return Match(b, s) && i == interface;
        }
    };

    /**
     * @brief Base data for controlling PCI devices.
     *
     * The bus, device, function numbers are used to identify a specific PCI device.
     * Other fields are used to store additional information about the device.
     */
    struct Device
    {
        uint8_t bus, device, function, header_type;
        ClassCode class_code;
    };

    /** @brief write integer to CONFIG_ADDRESS */
    void WriteAddress(uint32_t address);
    /** @brief write integer to CONFIG_DATA */
    void WriteData(uint32_t value);
    /** @brief read 32 bit integer from CONFIG_DATA */
    uint32_t ReadData();

    /** @brief read Vendor Id register (common for all header types) */
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
    /** @brief read Device Id register (common for all header types) */
    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
    /** @brief read Header Type register (common for all header types) */
    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);
    /** @brief read Class Code register (common for all header types) */
    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

    inline uint16_t ReadVendorId(const Device &dev)
    {
        return ReadVendorId(dev.bus, dev.device, dev.function);
    }

    /** @brief read 32 bit register on specified PCI device */
    uint32_t ReadConfReg(const Device &dev, uint8_t reg_addr);

    void WriteConfReg(const Device &dev, uint8_t reg_addr, uint32_t value);

    /**
     * @brief read Bus Numbers register (for header type 1)
     *
     * structure of 32 bit integer is following:
     * - 23:16: Subordinate Bus Number
     * - 15: 8: Secondary Bus Number
     * -  7: 0: Revision Number
     */
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

    /** @brief return true if the header_type is single function */
    bool IsSingleFunctionDevice(uint8_t header_type);

    /** @brief PCI device list found by ScanAllBus() */
    inline std::array<Device, 32> devices;
    /** @brief Number of valid elements in devices var */
    inline int num_device;

    /**
     * @brief Scan all PCI devices and store to devices var.
     *
     * This function scans all PCI devices from bus 0 recursively, writes them to devices.
     * It also sets the number of found devices to num_device var.
     */
    Error ScanAllBus();

    constexpr uint8_t CalcBarAddress(unsigned int bar_index)
    {
        return 0x10 + 4 * bar_index;
    }

    WithError<uint64_t> ReadBar(Device &device, unsigned int bar_index);
}