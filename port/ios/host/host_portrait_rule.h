/* Whether the game may turn to portrait on a screen (host_orientation.m). The menus fit their 640 columns to the
   screen's width in portrait, so a screen narrower than 640 points there draws a menu unit smaller than a point and
   its rows, 36 units tall, under 36 points: too small to tap. A device opts in (display.allow_portrait, its
   profile); each of its screens then qualifies by its own size, measured in points on its narrower side, and the
   answer is taken again whenever the game's scene moves to another screen. */
#ifndef HOST_PORTRAIT_RULE_H
#define HOST_PORTRAIT_RULE_H

#define HOST_PORTRAIT_MINIMUM_POINTS 640.0

static inline int host_portrait_fits(int device_allows, double screen_width, double screen_height)
{
    double narrow = screen_width < screen_height ? screen_width : screen_height;
    return device_allows && narrow >= HOST_PORTRAIT_MINIMUM_POINTS;
}

#endif
