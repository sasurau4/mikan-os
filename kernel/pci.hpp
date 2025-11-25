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

    /** @brief PCI ケーパビリティレジスタの共通ヘッダ */
    union CapabilityHeader
    {
        uint32_t data;
        struct
        {
            uint32_t cap_id : 8;
            uint32_t next_ptr : 8;
            uint32_t cap : 16;
        } __attribute__((packed)) bits;
    } __attribute__((packed));

    const uint8_t kCapabilityMSI = 0x05;
    const uint8_t kCapabilityMSIX = 0x11;

    /** @brief 指定された PCI デバイスの指定されたケーパビリティレジスタを読み込む
     *
     * @param dev  ケーパビリティを読み込む PCI デバイス
     * @param addr  ケーパビリティレジスタのコンフィグレーション空間アドレス
     */
    CapabilityHeader ReadCapabilityHeader(const Device &dev, uint8_t addr);

    /** @brief MSI ケーパビリティ構造
     *
     * MSI ケーパビリティ構造は 64 ビットサポートの有無などで亜種が沢山ある．
     * この構造体は各亜種に対応するために最大の亜種に合わせてメンバを定義してある．
     */
    struct MSICapability
    {
        union
        {
            uint32_t data;
            struct
            {
                uint32_t cap_id : 8;
                uint32_t next_ptr : 8;
                uint32_t msi_enable : 1;
                uint32_t multi_msg_capable : 3;
                uint32_t multi_msg_enable : 3;
                uint32_t addr_64_capable : 1;
                uint32_t per_vector_mask_capable : 1;
                uint32_t : 7;
            } __attribute__((packed)) bits;
        } __attribute__((packed)) header;

        uint32_t msg_addr;
        uint32_t msg_upper_addr;
        uint32_t msg_data;
        uint32_t mask_bits;
        uint32_t pending_bits;
    } __attribute__((packed));

    /** @brief MSI または MSI-X 割り込みを設定する
     *
     * @param dev  設定対象の PCI デバイス
     * @param msg_addr  割り込み発生時にメッセージを書き込む先のアドレス
     * @param msg_data  割り込み発生時に書き込むメッセージの値
     * @param num_vector_exponent  割り当てるベクタ数（2^n の n を指定）
     */
    Error ConfigureMSI(const Device &dev, uint32_t msg_addr, uint32_t msg_data,
                       unsigned int num_vector_exponent);

    enum class MSITriggerMode
    {
        kEdge = 0,
        kLevel = 1
    };

    enum class MSIDeliveryMode
    {
        kFixed = 0b000,
        kLowestPriority = 0b001,
        kSMI = 0b010,
        kNMI = 0b100,
        kINIT = 0b101,
        kExtINT = 0b111,
    };

    Error ConfigureMSIFixedDestination(
        const Device &dev, uint8_t apic_id,
        MSITriggerMode trigger_mode, MSIDeliveryMode delivery_mode,
        uint8_t vector, unsigned int num_vector_exponent);
}

void InitializePCI();