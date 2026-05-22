#pragma once

#include <cstddef>

/**
 * @brief Byte stream of the AmpMiddleKnob PNG file
 */
extern const unsigned char AmpMiddleKnob_png[];

/**
 * @brief Size of the byte stream of the AmpMiddleKnob PNG file
 */
constexpr unsigned int AmpMiddleKnob_pngSize = 422377;

/**
 * @brief Width of the AmpMiddleKnob film strip
 */
constexpr unsigned int AmpMiddleKnob_width = 140;
/**
 * @brief Length of the AmpMiddleKnob film strip
 */
constexpr unsigned int AmpMiddleKnob_length = 18060;

/**
 * @brief Number of pictures in the AmpMiddeKnob film strip (129)
 */
constexpr unsigned int AmpMiddleKnob_nPictures = AmpMiddleKnob_length / AmpMiddleKnob_width;
