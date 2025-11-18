/**
 * @file window.hpp
 *
 * @brief Window classes represent display area
 */

#pragma once

#include <vector>
#include <optional>
#include "graphics.hpp"

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
        virtual void Write(int x, int y, const PixelColor &c) override
        {
            window_.At(x, y) = c;
        };

        /** @brief Get the pixel of width for the window */
        virtual int Width() const override { return window_.Width(); };

        /** @brief Get the pixel of height for the window */
        virtual int Height() const override { return window_.Height(); };

    private:
        Window &window_;
    };

    Window(int width, int height);
    ~Window() = default;
    Window(const Window &) = delete;
    Window &operator=(const Window &rhs) = delete;

    /** @brief Draw the window to the specified PixelWriter
     *
     * @param writer PixelWriter to draw the window
     * @param position Position to draw the window starting from the top-left corner
     */
    void DrawTo(PixelWriter &writer, Vector2D<int> position);

    /** @brief Set transparent color */
    void SetTransparentColor(std::optional<PixelColor> c);

    /** @brief Get the writer for the instance */
    WindowWriter *Writer();

    /** @brief Get the pixel at the specified position */
    PixelColor &At(int x, int y);
    /** @brief Get the pixel at the specified position */
    const PixelColor &At(int x, int y) const;

    /** @brief Get the width of the window */
    int Width() const;
    /** @brief Get the height of the window */
    int Height() const;

private:
    int width_, height_;
    std::vector<std::vector<PixelColor>> data_{};
    WindowWriter writer_{*this};
    std::optional<PixelColor> transparent_color_{std::nullopt};
};
