/**
 * @file keyboard.hpp
 * @brief Keyboard handling for the kernel.
 */

#pragma once

#include <deque>
#include "message.hpp"

void InitializeKeyboard(std::deque<Message> &msg_queue);