#pragma once

#include <string>

// Struct which defines a button for the touch overlay. All buttons are
// drawn as and defined in the context of rectangles.
struct OverlayButton {
	// ID for the button
	int id_;
	// The name of the SpiceAPI input that should correspond to this button
	std::string input_name_;
	// Optional label for the button (menu nav buttons won't use this). If we shouldn't
	// draw a label, just set this to ""
	std::string label_;
	// The X coordinate of the center of the button
	int center_x_;
	// The Y coordinate of the width of the button
	int center_y_;
	// The width of the button
	int width_;
	// The heigth of the button
	int height_;
	// Says whether the button is rotated by 45 degrees or not
	bool rotated_;
	// Says whether the button is for a pinpad (these are handled differently on the API side)
	bool is_pinpad_;
};
