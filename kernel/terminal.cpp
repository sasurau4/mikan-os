#include "terminal.hpp"

#include <cstring>
#include <limits>

#include "font.hpp"
#include "layer.hpp"
#include "pci.hpp"
#include "asmfunc.h"
#include "elf.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"

#include "logger.hpp"

namespace
{
    std::vector<char *> MakeArgVector(char *command, char *first_arg)
    {
        std::vector<char *> argv;
        argv.push_back(command);
        if (!first_arg)
        {
            return argv;
        }

        char *p = first_arg;
        while (true)
        {
            while (isspace(p[0]))
            {
                ++p;
            }
            if (p[0] == 0)
            {
                break;
            }
            argv.push_back(p);

            while (p[0] != 0 && !isspace(p[0]))
            {
                ++p;
            }
            if (p[0] == 0)
            {
                break;
            }
            p[0] = 0;
            ++p;
        }

        return argv;
    }
}

Elf64_Phdr *GetProgramHeader(Elf64_Ehdr *ehdr, void *file_buf, int i)
{
    return reinterpret_cast<Elf64_Phdr *>(
        reinterpret_cast<uintptr_t>(file_buf) + ehdr->e_phoff + i * ehdr->e_phentsize);
}

/**
 * @brief Get the first load address of the ELF file.
 * The first load address is the lowest virtual address of the PT_LOAD segments.
 * Returning phdr.pvaddr of the first PT_LOAD segment is not correct because llvm-18 generates non-contiguous load segments.
 * @param ehdr Pointer to the ELF header.
 * @param file_buf Pointer to the buffer containing the ELF file.
 * @param first_addr Pointer to the variable to store the first load address like memcpy destination.
 */
void GetFirstLoadAddress(Elf64_Ehdr *ehdr, void *file_buf, uintptr_t *first_addr)
{
    for (int i = 0; i < ehdr->e_phnum; ++i)
    {
        const auto phdr = GetProgramHeader(ehdr, file_buf, i);

        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }
        *first_addr = phdr->p_vaddr;
        return;
    }
}

static_assert(kBytesPerFrame >= 4096);

WithError<PageMapEntry *> NewPageMap()
{
    auto frame = memory_manager->Allocate(1);
    if (frame.error)
    {
        return {nullptr, frame.error};
    }

    auto e = reinterpret_cast<PageMapEntry *>(frame.value.Frame());
    memset(e, 0, sizeof(uint64_t) * 512);
    return {e, MAKE_ERROR(Error::kSuccess)};
}

WithError<PageMapEntry *> SetNewPageMapIfNotPresent(PageMapEntry &entry)
{
    if (entry.bits.present)
    {
        return {entry.Pointer(), MAKE_ERROR(Error::kSuccess)};
    }

    auto [child_map, err] = NewPageMap();
    if (err)
    {
        return {nullptr, err};
    }

    entry.SetPointer(child_map);
    entry.bits.present = 1;

    return {child_map, MAKE_ERROR(Error::kSuccess)};
}

WithError<size_t> SetupPageMap(PageMapEntry *page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kpages)
{
    while (num_4kpages > 0)
    {
        const auto entry_index = addr.Part(page_map_level);

        auto [child_map, err] = SetNewPageMapIfNotPresent(page_map[entry_index]);
        if (err)
        {
            return {num_4kpages, err};
        }
        page_map[entry_index].bits.writable = 1;

        if (page_map_level == 1)
        {
            --num_4kpages;
        }
        else
        {
            auto [num_remain_pages, err] =
                SetupPageMap(child_map, page_map_level - 1, addr, num_4kpages);
            if (err)
            {
                return {num_4kpages, err};
            }
            num_4kpages = num_remain_pages;
        }

        if (entry_index == 511)
        {
            break;
        }

        addr.SetPart(page_map_level, entry_index + 1);
        for (int level = page_map_level - 1; level >= 1; --level)
        {
            addr.SetPart(level, 0);
        }
    }

    return {num_4kpages, MAKE_ERROR(Error::kSuccess)};
}

Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages)
{
    auto pml4_table = reinterpret_cast<PageMapEntry *>(GetCR3());
    return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

Error CopyLoadSegments(Elf64_Ehdr *ehdr, void *file_buf)
{
    /**
     * First loop to decide vaddr_min and vaddr_max for all PT_LOAD segments because llvm-18 generates non-contiguous load segments.
     *
     * Ex.
     *   Segment 1: vaddr = 0x0000, memsz = 1244（Page 0内）
     *   Segment 2: vaddr = 0x14e0, memsz = 1070（Page 1内）
     *   Segment 3: vaddr = 0x2910, memsz = 2720（Page 2〜3にまたがる）
     * In this case, vaddr_min = 0x0000, vaddr_max = 0x2910 + 2720 = 0x3c30, total_range = 0x3c30, num_4kpages = 4.
     * but sum memsz = 1244 + 1070 + 2720 = 5034 is num_4kpages = 2 and smaller than total_range = 0x3c30 = 15600 because of non-contiguous segments.
     * To avoid this problem, we need to calculate vaddr_min and vaddr_max first and allocate memory for the whole range, then copy each segment to the allocated memory.
     */
    uint64_t vaddr_min = 0xffffffffffffffff;
    uint64_t vaddr_max = 0;
    for (int i = 0; i < ehdr->e_phnum; ++i)
    {
        auto phdr = GetProgramHeader(ehdr, file_buf, i);
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }
        vaddr_min = std::min(vaddr_min, phdr->p_vaddr);
        vaddr_max = std::max(vaddr_max, phdr->p_vaddr + phdr->p_memsz);
    }

    const auto total_range = vaddr_max - vaddr_min;
    LinearAddress4Level dest_addr;
    dest_addr.value = vaddr_min;
    const auto num_4kpages = (total_range + 4095) / 4096;

    // Setup page maps
    if (auto err = SetupPageMaps(dest_addr, num_4kpages))
    {
        return err;
    }

    // Second loop to copy each PT_LOAD segment to memory
    for (int i = 0; i < ehdr->e_phnum; ++i)
    {
        auto phdr = GetProgramHeader(ehdr, file_buf, i);
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }

        const auto src = reinterpret_cast<uint8_t *>(file_buf) + phdr->p_offset;
        const auto dst = reinterpret_cast<uint8_t *>(phdr->p_vaddr);
        memcpy(dst, src, phdr->p_filesz);
        memset(dst + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
    }
    return MAKE_ERROR(Error::kSuccess);
}

/**
 * @brief Load an ELF executable into memory.
 *
 * @param ehdr Pointer to the ELF header.
 * Elf header is the dictionary of the ELF file buffer, DO NOT access data from it directly.
 * Instead, use the file buffer.
 * @param file_buf Pointer to the buffer containing the ELF file.
 * @return Error Error code indicating success or failure.
 */
Error LoadElf(Elf64_Ehdr *ehdr, void *file_buf)
{
    if (ehdr->e_type != ET_EXEC)
    {
        return MAKE_ERROR(Error::kInvalidFormat);
    }

    uintptr_t addr_first = 0;
    GetFirstLoadAddress(ehdr, file_buf, &addr_first);
    if (addr_first < 0xffff'8000'0000'0000)
    {
        return MAKE_ERROR(Error::kInvalidFormat);
    }

    if (auto err = CopyLoadSegments(ehdr, file_buf))
    {
        return err;
    }

    return MAKE_ERROR(Error::kSuccess);
}

Error CleanPageMap(PageMapEntry *page_map, int page_map_level)
{
    for (int i = 0; i < 512; ++i)
    {
        auto entry = page_map[i];
        if (!entry.bits.present)
        {
            continue;
        }

        if (page_map_level > 1)
        {
            if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1))
            {
                return err;
            }
        }

        const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
        const FrameID map_frame{entry_addr / kBytesPerFrame};
        if (auto err = memory_manager->Free(map_frame, 1))
        {
            return err;
        }
        page_map[i].data = 0;
    }

    return MAKE_ERROR(Error::kSuccess);
}

Error CleanPageMaps(LinearAddress4Level addr)
{
    auto pml4_table = reinterpret_cast<PageMapEntry *>(GetCR3());
    auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
    pml4_table[addr.parts.pml4].data = 0;
    if (auto err = CleanPageMap(pdp_table, 3))
    {
        return err;
    }
    const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
    const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
    return memory_manager->Free(pdp_frame, 1);
}

Terminal::Terminal()
{
    window_ = std::make_shared<ToplevelWindow>(
        kColumns * 8 + 8 + ToplevelWindow::kMarginX,
        kRows * 16 + 8 + ToplevelWindow::kMarginY,
        screen_config.pixel_format,
        "MikanTerm");
    DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

    layer_id_ = layer_manager
                    ->NewLayer()
                    .SetWindow(window_)
                    .SetDraggable(true)
                    .ID();

    Print(">");
    cmd_history_.resize(8);
}

Rectangle<int> Terminal::BlinkCursor()
{
    cursor_visible_ = !cursor_visible_;
    DrawCursor(cursor_visible_);

    return {CalcCursorPos(), {7, 15}};
}

void Terminal::DrawCursor(bool visible)
{
    const auto color = visible ? ToColor(0xffffff) : ToColor(0);
    FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
}

Vector2D<int> Terminal::CalcCursorPos() const
{
    return ToplevelWindow::kTopLeftMargin +
           Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

Rectangle<int> Terminal::InputKey(
    uint8_t modifier, uint8_t keycode, char ascii)
{
    DrawCursor(false);

    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n')
    {
        linebuf_[linebuf_index_] = 0;
        if (linebuf_index_ > 0)
        {
            cmd_history_.pop_back();
            cmd_history_.push_front(linebuf_);
        }
        linebuf_index_ = 0;
        cmd_history_index_ = -1;

        cursor_.x = 0;
        if (cursor_.y < kRows - 1)
        {
            ++cursor_.y;
        }
        else
        {
            Scroll1();
        }
        ExecuteLine();
        Print(">");
        draw_area.pos = ToplevelWindow::kTopLeftMargin;
        draw_area.size = window_->InnerSize();
    }
    else if (ascii == '\b')
    {
        if (cursor_.x > 0)
        {
            --cursor_.x;
            FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
            draw_area.pos = CalcCursorPos();

            if (linebuf_index_ > 0)
            {
                --linebuf_index_;
            }
        }
    }
    else if (ascii != 0)
    {
        if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1)
        {
            linebuf_[linebuf_index_] = ascii;
            ++linebuf_index_;
            WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
            ++cursor_.x;
        }
    }
    else if (keycode == 0x51)
    {
        draw_area = HistoryUpDown(-1); // down arrow
    }
    else if (keycode == 0x52)
    {
        draw_area = HistoryUpDown(1); // up arrow
    }

    DrawCursor(true);

    return draw_area;
};

void Terminal::Scroll1()
{
    Rectangle<int> move_src{
        ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
        {8 * kColumns, 16 * (kRows - 1)}};
    window_->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
    FillRectangle(*window_->InnerWriter(),
                  {4, 4 + 16 * cursor_.y}, {8 * kColumns, 16}, {0, 0, 0});
}

void Terminal::ExecuteLine()
{
    char *command = &linebuf_[0];
    char *first_arg = strchr(&linebuf_[0], ' ');
    if (first_arg)
    {
        *first_arg = 0;
        ++first_arg;
    }

    if (strcmp(command, "echo") == 0)
    {
        if (first_arg)
        {
            Print(first_arg);
        }
        Print("\n");
    }
    else if (strcmp(command, "clear") == 0)
    {
        FillRectangle(*window_->InnerWriter(),
                      {4, 4}, {8 * kColumns, 16 * kRows}, {0, 0, 0});
        cursor_.y = 0;
    }
    else if (strcmp(command, "lspci") == 0)
    {
        char s[64];
        for (int i = 0; i < pci::num_device; ++i)
        {
            const auto &dev = pci::devices[i];
            auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
            sprintf(s, "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
                    dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
                    dev.class_code.base, dev.class_code.sub, dev.class_code.interface);
            Print(s);
        }
    }
    else if (strcmp(command, "ls") == 0)
    {
        auto root_dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(
            fat::boot_volume_image->root_cluster);
        auto entries_per_cluster =
            fat::bytes_per_cluster / sizeof(fat::DirectoryEntry);
        char base[9], ext[4];
        char s[64];
        for (int i = 0; i < entries_per_cluster; ++i)
        {
            ReadName(root_dir_entries[i], base, ext);
            if (base[0] == 0x00)
            {
                break;
            }
            else if (static_cast<uint8_t>(base[0]) == 0xe5)
            {
                continue;
            }
            else if (root_dir_entries[i].attr == fat::Attribute::kLongName)
            {
                continue;
            }

            if (ext[0])
            {
                sprintf(s, "%s.%s\n", base, ext);
            }
            else
            {
                sprintf(s, "%s\n", base);
            }
            Print(s);
        }
    }
    else if (strcmp(command, "cat") == 0)
    {
        char s[64];

        auto file_entry = fat::FindFile(first_arg);
        if (!file_entry)
        {
            sprintf(s, "no such file: %s\n", first_arg);
            Print(s);
        }
        else
        {
            auto cluster = file_entry->FirstCluster();
            auto remain_bytes = file_entry->file_size;

            DrawCursor(false);

            while (cluster != 0 && cluster != fat::kEndOfClusterchain)
            {
                char *p = fat::GetSectorByCluster<char>(cluster);

                int i = 0;
                for (; i < fat::bytes_per_cluster && i < remain_bytes; ++i)
                {
                    Print(*p);
                    ++p;
                }
                remain_bytes -= i;
                cluster = fat::NextCluster(cluster);
            }
            DrawCursor(true);
        }
    }
    else if (command[0] != 0)
    {
        auto file_entry = fat::FindFile(command);
        if (!file_entry)
        {
            Print("no such command: ");
            Print(command);
            Print("\n");
        }
        else if (auto err = ExecuteFile(*file_entry, command, first_arg))
        {
            Print("failed to exec file: ");
            Print(err.Name());
            Print("\n");
        }
    }
}

Error Terminal::ExecuteFile(const fat::DirectoryEntry &file_entry, char *command, char *first_arg)
{
    std::vector<uint8_t> file_buf(file_entry.file_size);
    fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

    // Copy buffer to struct because llvm-18 check alignment violation more strictly than llvm-7
    Elf64_Ehdr elf_header_obj;
    // // copy &file_buf[0] to &elf_header_obj only 64 (Elf64_Ehdr size) bytes
    memcpy(&elf_header_obj, &file_buf[0], sizeof(Elf64_Ehdr));
    auto elf_header = &elf_header_obj;
    if (memcmp(elf_header->e_ident, "\x7f"
                                    "ELF",
               4) != 0)
    {
        using Func = void();
        auto f = reinterpret_cast<Func *>(&file_buf[0]);
        f();
        return MAKE_ERROR(Error::kSuccess);
    }

    auto argv = MakeArgVector(command, first_arg);
    if (auto err = LoadElf(elf_header, &file_buf[0]))
    {
        return err;
    }

    auto entry_addr = elf_header->e_entry;

    using Func = int(int, char **);
    auto f = reinterpret_cast<Func *>(entry_addr);
    auto ret = f(argv.size(), &argv[0]);

    char s[64];
    sprintf(s, "app exited. ret = %d\n", ret);
    Print(s);

    uintptr_t addr_first = 0;
    GetFirstLoadAddress(elf_header, &file_buf[0], &addr_first);
    if (auto err = CleanPageMaps(LinearAddress4Level{addr_first}))
    {
        return err;
    }

    return MAKE_ERROR(Error::kSuccess);
}

void Terminal::Print(char c)
{
    auto newline = [this]()
    {
        cursor_.x = 0;
        if (cursor_.y < kRows - 1)
        {
            ++cursor_.y;
        }
        else
        {
            Scroll1();
        }
    };

    if (c == '\n')
    {
        newline();
    }
    else
    {
        WriteAscii(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
        if (cursor_.x == kColumns - 1)
        {
            newline();
        }
        else
        {
            ++cursor_.x;
        }
    }
}

void Terminal::Print(const char *s)
{
    DrawCursor(false);

    while (*s)
    {
        Print(*s);
        ++s;
    }

    DrawCursor(true);
}

Rectangle<int> Terminal::HistoryUpDown(int direction)
{
    if (direction == -1 && cmd_history_index_ >= 0)
    {
        --cmd_history_index_;
    }
    else if (direction == 1 && cmd_history_index_ + 1 < cmd_history_.size())
    {
        ++cmd_history_index_;
    }

    cursor_.x = 1;
    const auto first_pos = CalcCursorPos();

    Rectangle<int> draw_area{first_pos, {8 * (kColumns - 1), 16}};
    FillRectangle(*window_->Writer(), draw_area.pos, draw_area.size, {0, 0, 0});

    const char *history = "";
    if (cmd_history_index_ >= 0)
    {
        history = &cmd_history_[cmd_history_index_][0];
    }

    strcpy(&linebuf_[0], history);
    linebuf_index_ = strlen(history);

    WriteString(*window_->Writer(), first_pos, history, {255, 255, 255});
    cursor_.x = linebuf_index_ + 1;
    return draw_area;
}

void TaskTerminal(uint64_t task_id, int64_t data)
{
    __asm__("cli");
    Task &task = task_manager->CurrentTask();
    Terminal *terminal = new Terminal;
    layer_manager->Move(terminal->LayerID(), {100, 200});
    active_layer->Activate(terminal->LayerID());
    layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
    __asm__("sti");

    while (true)
    {
        __asm__("cli");
        auto msg = task.ReceiveMessage();
        if (!msg)
        {
            task.Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        switch (msg->type)
        {
        case Message::kTimerTimeout:
        {
            const auto area = terminal->BlinkCursor();
            Message msg = MakeLayerMessage(
                task_id, terminal->LayerID(),
                LayerOperation::DrawArea, area);
            __asm__("cli");
            task_manager->SendMessage(1, msg);
            __asm__("sti");
            break;
        }

        case Message::kKeyPush:
        {
            const auto area = terminal->InputKey(
                msg->arg.keyboard.modifier,
                msg->arg.keyboard.keycode,
                msg->arg.keyboard.ascii);
            Message msg = MakeLayerMessage(
                task_id, terminal->LayerID(),
                LayerOperation::DrawArea, area);
            __asm__("cli");
            task_manager->SendMessage(1, msg);
            __asm__("sti");
            break;
        }
        default:
        {
            break;
        }
        }
    }
}