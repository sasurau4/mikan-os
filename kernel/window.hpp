/**
 * @file window.hpp
 *
 * @brief Window classes represent display area
 */

#pragma once

#include <vector>
#include <optional>
#include <string>
#include "graphics.hpp"
#include "frame_buffer.hpp"

/**
 * @brief Window class represents a display area on the screen.
 *
 * This includes not only the window hold title and menu,
 * but also the display area for mouse cursor.
 */
class Window
{
public:
    class WindowWriter : public PixelWriter
    {
    public:
        WindowWriter(Window &window) : window_{window} {}
        /** @brief Write the specified color to the specified point */
        virtual void Write(Vector2D<int> pos, const PixelColor &c) override
        {
            window_.Write(pos, c);
        };

        /** @brief Get the pixel of width for the window */
        virtual int Width() const override { return window_.Width(); };

        /** @brief Get the pixel of height for the window */
        virtual int Height() const override { return window_.Height(); };

    private:
        Window &window_;
    };

    Window(int width, int height, PixelFormat shadow_format);
    virtual ~Window() = default;
    Window(const Window &) = delete;
    Window &operator=(const Window &rhs) = delete;

    /** @brief Draw the window to the specified FrameBuffer
     *
     * @param dst FrameBuffer to draw the window
     * @param pos Position to draw the window starting from the top-left corner of the dst
     * @param area Area inside the window to draw from the top-left corner of the the dst
     */
    void DrawTo(FrameBuffer &dst, Vector2D<int> pos, const Rectangle<int> &area);

    /** @brief Set transparent color */
    void SetTransparentColor(std::optional<PixelColor> c);

    /** @brief Get the writer for the instance */
    WindowWriter *Writer();

    /** @brief Get the pixel at the specified position */
    const PixelColor &At(Vector2D<int> pos) const;
    /** @brief Write the pixel at the specified position */
    void Write(Vector2D<int> pos, PixelColor c);

    /** @brief Get the width of the window */
    int Width() const;
    /** @brief Get the height of the window */
    int Height() const;
    /** @brief Get the size of the window */
    Vector2D<int> Size() const;

    /**
     * @brief Move the rectangle to the specified position inside the window
     *
     * @param dst_pos Destination position to move
     * @param src Source rectangle to move
     */
    void Move(Vector2D<int> dst_pos, const Rectangle<int> &src);

    virtual void Activate() {}
    virtual void Deactivate() {}

private:
    int width_, height_;
    std::vector<std::vector<PixelColor>> data_{};
    WindowWriter writer_{*this};
    std::optional<PixelColor> transparent_color_{std::nullopt};

    FrameBuffer shadow_buffer_{};
};

class ToplevelWindow : public Window
{
public:
    static constexpr Vector2D<int> kTopLeftMargin{4, 24};
    static constexpr Vector2D<int> kBottomRightMargin{4, 4};

    class InnerAreaWriter : public PixelWriter
    {
    public:
        InnerAreaWriter(ToplevelWindow &window) : window_{window} {}
        virtual void Write(Vector2D<int> pos, const PixelColor &c) override
        {
            window_.Write(pos + kTopLeftMargin, c);
        }
        virtual int Width() const override
        {
            return window_.Width() - kTopLeftMargin.x - kBottomRightMargin.x;
        }
        virtual int Height() const override
        {
            return window_.Height() - kTopLeftMargin.y - kBottomRightMargin.y;
        }

    private:
        ToplevelWindow &window_;
    };

    ToplevelWindow(int width, int height, PixelFormat shadow_format,
                   const std::string &title);

    virtual void Activate() override;
    virtual void Deactivate() override;

    InnerAreaWriter *InnerWriter() { return &inner_writer_; }
    Vector2D<int> InnerSize() const;

private:
    std::string title_;
    InnerAreaWriter inner_writer_{*this};
};

void DrawWindow(PixelWriter &writer, const char *title);
void DrawTextBox(PixelWriter &writer, Vector2D<int> pos, Vector2D<int> size);
void DrawWindowTitle(PixelWriter &writer, const char *title, bool active);
