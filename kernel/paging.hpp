/**
 * @file paging.hpp
 * @brief Paging management for the kernel.
 */
#pragma once

#include <cstddef>

/**
 * @brief Number of page directories acquired statically
 *
 * This constant used by SetupIdentityPageMap function
 * A page directory can map 512 pieces of 2 MiB pages
 * The virtual address of kPageDirectoryCount * 1GiB are mapped
 */
const size_t kPageDirectoryCount = 64;

/**
 * @brief Sets up an identity page table with virtual addresses to physical addresses.
 * Finally, CR3 register is set to the address of the page table
 */
void SetupIdentityPageTable();