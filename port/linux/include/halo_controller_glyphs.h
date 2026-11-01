/* Controller prompt identities shared by the guest and native raster service. */
#ifndef HALO_CONTROLLER_GLYPHS_H
#define HALO_CONTROLLER_GLYPHS_H

enum { HALO_GLYPH_ORIGINAL, HALO_GLYPH_HIDDEN, HALO_GLYPH_SYMBOL, HALO_GLYPH_KEYCAP };

/* Icon indices are HUD identities, not frames in the maps' sprite atlases. */
static inline int halo_glyph_keyboard_action(int semantic)
{
	static const signed char actions[] = {13, 7, 6, 14, 4, 8, 11, 12, 5, 10};
	return semantic >= 18 && semantic < 28 ? actions[semantic - 18] : -1;
}

static inline int halo_glyph_is_axis(int physical)
{
	return physical == 6 || physical == 7 || physical == 16 || physical == 17;
}

/* SDL3 button and axis enum values: White/Black are modern shoulders. */
static inline int halo_glyph_element(int physical)
{
	static const signed char elements[] = {0, 1, 2, 3, 10, 9, 4, 5, 11, 12, 13, 14, 6, 4, 7, 8, 0, 2};
	return physical >= 0 && physical < 18 ? elements[physical] : -1;
}

int halo_controller_glyph_request(short player, short semantic, short physical, char *label, unsigned int size);
struct host_ui_pointer;
void halo_controller_glyph_pointer_activity(int menus, struct host_ui_pointer const *pointer);
#endif
