/**
 * @file layer.hpp
 *
 * @brief Layer class definition
 */

#pragma once

#include <memory>
#include <map>
#include <vector>

#include "graphics.hpp"
#include "window.hpp"

/**
 * @brief Represents a graphical layer with a unique identifier.
 *
 * Currently, this class only holds one window,
 * but it can be extended for the future
 */
class Layer
{
public:
    /** @brief Constructs a Layer with a unique identifier. */
    Layer(unsigned int id = 0);
    /** @brief Returns the unique identifier of the Layer. */
    unsigned int ID() const;

    /** @brief Sets the window associated with the Layer. The window associated with the Layer is replaced. */
    Layer &SetWindow(const std::shared_ptr<Window> &window);
    /** @brief Returns the window associated with the Layer. */
    std::shared_ptr<Window> GetWindow() const;

    /** @brief Moves the Layer to a new absolute position. This function does not re-render */
    Layer &Move(Vector2D<int> pos);
    /** @brief Moves the Layer relative to its current position. This function does not re-render */
    Layer &MoveRelative(Vector2D<int> pos_diff);

    /** @brief Draws the Layer to the specified PixelWriter. */
    void DrawTo(PixelWriter &writer) const;

private:
    unsigned int id_;
    Vector2D<int> pos_;
    std::shared_ptr<Window> window_;
};

/** @brief Manages multiple Layers. */
class LayerManager
{
public:
    /** @brief Sets the PixelWriter for the LayerManager. */
    void SetWriter(PixelWriter *writer);

    /** @brief Creates a new Layer and adds it to the LayerManager
     * The created layer is hold by LayerManager.
     * @return Reference to the newly created Layer
     */
    Layer &NewLayer();

    /** @brief Draws the current displayed layer */
    void Draw() const;

    /** @brief Moves a layer to a new absolute position. This function does not re-render */
    void Move(unsigned int id, Vector2D<int> new_position);
    /** @brief Moves a layer relative to its current position. This function does not re-render */
    void MoveRelative(unsigned int id, Vector2D<int> pos_diff);

    /** @brief Changes the height of a layer.
     *
     * If new_height is negative value, the layer is hidden.
     * If new_height is large or equal to 0, the layer is set to that height.
     * If new_height is larger than the current maximum height, the layer is set to the most front layer.
     */
    void UpDown(unsigned int id, int new_height);
    /** @brief Hides the layer. */
    void Hide(unsigned int id);

private:
    PixelWriter *writer_{nullptr};
    std::vector<std::unique_ptr<Layer>> layers_{};
    std::vector<Layer *> layer_stack_{};
    unsigned int latest_id_{0};

    Layer *FindLayer(unsigned int id);
};

extern LayerManager *layer_manager;